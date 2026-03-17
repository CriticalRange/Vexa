package com.critical.vexaemulator.auth

import android.app.Activity
import android.content.Intent
import android.net.Uri
import android.util.Log
import androidx.browser.customtabs.CustomTabsIntent
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.json.JSONObject
import java.net.InetSocketAddress
import java.net.ServerSocket
import java.net.Socket
import java.net.SocketTimeoutException
import java.net.URLEncoder
import java.nio.charset.StandardCharsets
import java.security.MessageDigest
import java.security.SecureRandom
import java.util.Base64
import java.util.concurrent.TimeoutException

class OAuthPkceClient {
    companion object {
        const val AUTH_ENDPOINT =
            "https://oauth.accounts.hytale.com/oauth2/auth"
        const val TOKEN_ENDPOINT =
            "https://oauth.accounts.hytale.com/oauth2/token"
        const val USERINFO_ENDPOINT =
            "https://oauth.accounts.hytale.com/userinfo"

        const val DEFAULT_CLIENT_ID = "hytale-launcher"
        const val DEFAULT_REDIRECT_URI = "https://accounts.hytale.com/consent/client"
        const val DEFAULT_SCOPE = "openid offline auth:launcher"

        private const val CALLBACK_HOST =
            "127.0.0.1"
        private const val CALLBACK_PATH = "/authorization-callback"
        private const val TAG = "Vexa-OAuth"
    }

    suspend fun login(
        activity: Activity,
        clientId: String = DEFAULT_CLIENT_ID,
        redirectUri: String = DEFAULT_REDIRECT_URI,
        scope: String = DEFAULT_SCOPE,
    ): TokenResponse {
        val verifier =
            base64UrlNoPad(randomBytes(48))
        val challenge =
            base64UrlNoPad(
                MessageDigest.getInstance("SHA-256")
                    .digest(verifier.toByteArray(StandardCharsets.US_ASCII))
            )
        val stateInner =
            base64UrlNoPad(randomBytes(24))

        val callback =
            LoopbackServer(stateInner).use { loopback ->
                val port = loopback.start()
                val stateEncoded = base64UrlNoPad(
                    JSONObject()
                        .put("state", stateInner)
                        .put("port", port.toString())
                        .toString()
                        .toByteArray(StandardCharsets.UTF_8)
                )

                val authUrl = buildAuthUrl(
                    clientId = clientId,
                    redirectUri = redirectUri,
                    scope = scope,
                    state = stateEncoded,
                    challenge = challenge,
                )

                withContext(Dispatchers.Main) {
                    openInBrowser(activity, authUrl)
                }

                withContext(Dispatchers.IO) {
                    loopback.waitForCallback(
                        timeoutMs =
                            5 * 60_000L
                    )
                }
            }

        if (!callback.error.isNullOrBlank()) {
            throw IllegalStateException("OAuth error: ${callback.error} - ${callback.errorDescription.orEmpty()}")
        }
        if (callback.code.isNullOrBlank()) {
            throw IllegalStateException("OAuth callback did not contain authorization code")
        }
        if (callback.state != stateInner) {
            throw IllegalStateException("OAuth state mismatch")
        }

        return exchangeCode(
            clientId = clientId,
            redirectUri = redirectUri,
            code = callback.code,
            verifier = verifier,
        )
    }

    suspend fun refresh(
        refreshToken: String,
        clientId: String = DEFAULT_CLIENT_ID
    ): TokenResponse {
        val result = AuthHttp.postForm(
            url = TOKEN_ENDPOINT,
            form = mapOf(
                "grant_type" to "refresh_token",
                "client_id" to clientId,
                "refresh_token" to refreshToken,
            )
        )

        if (result.status !in 200..299) {
            throw IllegalStateException(
                "refresh_token grant failed:${result.status} ${
                    result.body.take(
                        300
                    )
                }"
            )
        }
        return parseTokenResponse(result.body)
    }

    private suspend fun exchangeCode(
        clientId: String,
        redirectUri: String,
        code: String,
        verifier: String,
    ): TokenResponse {
        val result = AuthHttp.postForm(
            url = TOKEN_ENDPOINT,
            form = mapOf(
                "grant_type" to
                        "authorization_code",
                "client_id" to clientId,
                "redirect_uri" to redirectUri,
                "code" to code,
                "code_verifier" to verifier,
            )
        )

        if (result.status !in 200..299) {
            throw IllegalStateException(
                "authorization_code exchange failed: ${result.status} ${
                    result.body.take(
                        300
                    )
                }"
            )
        }
        return parseTokenResponse(result.body)
    }

    private fun parseTokenResponse(body: String):
            TokenResponse {
        val json = JSONObject(body)
        return TokenResponse(
            accessToken =
                json.optString("access_token", "").ifBlank { null },
            refreshToken =
                json.optString("refresh_token", "").ifBlank
                { null },
            idToken = json.optString(
                "id_token",
                ""
            ).ifBlank { null },
            tokenType = json.optString(
                "token_type",
                ""
            ).ifBlank { null },
            expiresIn = when (val raw = json.opt("expires_in")) {
                is Number -> raw.toLong()
                is String -> raw.toLongOrNull()
                else -> null
            },
            scope = json.optString(
                "scope",
                ""
            ).ifBlank { null },
        )
    }

    private fun openInBrowser(
        activity: Activity,
        authUrl: String
    ) {
        val uri = Uri.parse(authUrl)
        runCatching {
            CustomTabsIntent.Builder()
                .setShowTitle(true)
                .build()
                .launchUrl(activity, uri)
        }.onFailure {

            activity.startActivity(
                Intent(
                    Intent.ACTION_VIEW,
                    uri
                )
            )
        }
    }

    private fun buildAuthUrl(
        clientId: String,
        redirectUri: String,
        scope: String,
        state: String,
        challenge: String,
    ): String {
        val query = linkedMapOf(
            "response_type" to "code",
            "client_id" to clientId,
            "redirect_uri" to redirectUri,
            "scope" to scope,
            "state" to state,
            "code_challenge" to challenge,
            "code_challenge_method" to "S256",
            "max_age" to "0",
        )

        val encoded =
            query.entries.joinToString("&") { (k, v) ->
                "${URLEncoder.encode(k, "UTF-8")}=${URLEncoder.encode(v, "UTF-8")}"
            }
        return "$AUTH_ENDPOINT?$encoded"
    }

    private fun randomBytes(size: Int): ByteArray =
        ByteArray(size).also {
            SecureRandom().nextBytes(it)
        }

    private fun base64UrlNoPad(data: ByteArray):
            String {
        return Base64.getUrlEncoder().withoutPadding().encodeToString(data)
    }

    private data class CallbackResult(
        val code: String?,
        val state: String?,
        val error: String?,
        val errorDescription: String?,
    )

    private class LoopbackServer(
        private val
        expectedState: String
    ) : AutoCloseable {
        private val server = ServerSocket()

        fun start(): Int {
            server.reuseAddress = true

            server.bind(InetSocketAddress(CALLBACK_HOST, 0))
            return server.localPort
        }

        fun waitForCallback(timeoutMs: Long):
                CallbackResult {
            val deadline =
                System.currentTimeMillis() + timeoutMs
            while (System.currentTimeMillis() <
                deadline
            ) {
                val remaining = (deadline -
                        System.currentTimeMillis()).coerceAtLeast(1L).coerceAtMost(1000L)
                server.soTimeout = remaining.toInt()
                try {
                    server.accept().use { socket ->
                        val result =
                            handleClient(socket)
                        if (result != null) return result
                    }
                } catch (_: SocketTimeoutException) {
                }
            }
            throw TimeoutException("Timed out waiting for OAuth callback")
        }

        private fun handleClient(socket: Socket):
                CallbackResult? {
            val reader =
                socket.getInputStream().bufferedReader(StandardCharsets.US_ASCII)
            val requestLine =
                reader.readLine().orEmpty()
            if (requestLine.isBlank()) return null

            Log.i(TAG, "Loopback: $requestLine")

            var line: String?
            do {
                line = reader.readLine()
            } while (!line.isNullOrEmpty())

            val parts = requestLine.split(" ")
            val method =
                parts.getOrNull(0).orEmpty()
            val target =
                parts.getOrNull(1).orEmpty()

            val corsHeaders = linkedMapOf(
                "Access-Control-Allow-Origin" to
                        "*",
                "Access-Control-Allow-Private-Network" to "true",
                "Access-Control-Allow-Methods" to
                        "GET, OPTIONS",
                "Access-Control-Allow-Headers" to
                        "*",
                "Connection" to "close",
            )

            if (method.equals(
                    "OPTIONS", ignoreCase
                    = true
                )
            ) {
                Log.i(TAG, "Loopback: responding to PNA preflight OPTIONS")
                writeResponse(socket, "HTTP/1.1 204 No Content", corsHeaders, "")
                return null
            }

            var result: CallbackResult? = null
            var gotCode = false
            var uri = Uri.parse("http://$CALLBACK_HOST$target")

            if (parts.size >= 2) {
                uri = if (target.startsWith("http://") || target.startsWith("https://")) {
                    Uri.parse(target)
                } else {
                    Uri.parse("http://$CALLBACK_HOST$target")
                }
            }

            val code = uri.getQueryParameter("code")
            val state =
                uri.getQueryParameter("state")
            val error =
                uri.getQueryParameter("error")
            val errorDescription =
                uri.getQueryParameter("error_description")
            val stateMatches = state ==
                    expectedState

            if (!error.isNullOrBlank() &&
                stateMatches
            ) {
                gotCode = true
                result = CallbackResult(null, state, error, errorDescription)
                writeResponse(socket, "HTTP/1.1 200 OK", corsHeaders, successBody())
                return CallbackResult(
                    null, state,
                    error, errorDescription
                )
            } else if (!code.isNullOrBlank() &&
                stateMatches
            ) {
                gotCode = true
                result = CallbackResult(null, state, error, errorDescription)
                writeResponse(socket, "HTTP/1.1 200 OK", corsHeaders, successBody())
                return CallbackResult(
                    code, state,
                    null, null
                )
            } else if (!code.isNullOrBlank()) {
                gotCode = true
                result = CallbackResult(null, state, error, errorDescription)
                Log.w(TAG, "Ignoring stale callback state=$state expected=$expectedState")
            }

            if (gotCode) {
                writeResponse(
                    socket, "HTTP/1.1 200 OK",
                    corsHeaders, successBody()
                )
            } else {
                writeResponse(
                    socket, "HTTP/1.1 200 OK",
                    corsHeaders, ""
                )
            }

            return result
        }

        private fun successBody(): String {
            return "<html><head><meta http-equiv='refresh' content='0;url=vexa://oauth/done'></head>" +

                    "<body><script>window.location='vexa://oauth/done';</script></body></html>"
        }

        private fun writeResponse(
            socket: Socket,
            statusLine: String,
            headers: Map<String, String>,
            body: String,
        ) {
            val bytes =
                body.toByteArray(StandardCharsets.UTF_8)
            val sb = StringBuilder()
            sb.append(statusLine).append("\r\n")
            headers.forEach { (k, v) ->
                sb.append(k).append(": ").append(v).append("\r\n")
            }
            if (body.isNotEmpty())
                sb.append("Content-Type: text/html; charset=utf-8\r\n")
            sb.append("Content-Length: ").append(bytes.size).append("\r\n\r\n")

            val out = socket.getOutputStream()

            out.write(
                sb.toString().toByteArray(
                    StandardCharsets
                        .UTF_8
                )
            )
            if (bytes.isNotEmpty()) out.write(bytes)
            out.flush()
        }

        override fun close() {
            runCatching { server.close() }
        }
    }
}