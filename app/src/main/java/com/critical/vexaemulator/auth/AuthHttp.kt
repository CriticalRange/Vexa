package com.critical.vexaemulator.auth

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.withContext
import java.net.HttpURLConnection
import java.net.UnknownHostException
import java.net.URL
import java.net.URLEncoder
import java.nio.charset.StandardCharsets

internal object AuthHttp {
    data class Result(
        val status: Int, val body:
        String
    )

    suspend fun get(
        url: String, headers:
        Map<String, String> = emptyMap()
    ): Result =
        withContext(Dispatchers.IO) {
            dnsAwareRequest(url) {
                val conn = (URL(url).openConnection() as
                        HttpURLConnection).apply {
                    requestMethod = "GET"
                    connectTimeout = 15_000
                    readTimeout = 15_000
                    doInput = true
                    headers.forEach { (k, v) ->
                        setRequestProperty(k, v)
                    }
                }
                conn.useAndRead()
            }
        }

    suspend fun postForm(
        url: String, form:
        Map<String, String>, headers: Map<String, String> =
            emptyMap()
    ): Result =
        withContext(Dispatchers.IO) {
            dnsAwareRequest(url) {
                val payload =
                    form.entries.joinToString("&") { (k, v) ->
                        "${URLEncoder.encode(k, "UTF-8")}=${URLEncoder.encode(v, "UTF-8")}"
                    }.toByteArray(StandardCharsets.UTF_8)

                val conn = (URL(url).openConnection() as
                        HttpURLConnection).apply {
                    requestMethod = "POST"
                    connectTimeout = 15_000
                    readTimeout = 15_000
                    doInput = true
                    doOutput = true
                    setRequestProperty(
                        "Content-Type",
                        "application/x-www-form-urlencoded"
                    )
                    setRequestProperty(
                        "Accept",
                        "application/json"
                    )
                    headers.forEach { (k, v) ->
                        setRequestProperty(k, v)
                    }
                }

                conn.outputStream.use {
                    it.write(payload)
                }
                conn.useAndRead()
            }
        }

    suspend fun postJson(
        url: String, jsonBody:
        String, headers: Map<String, String> = emptyMap()
    ):
            Result =
        withContext(Dispatchers.IO) {
            dnsAwareRequest(url) {
                val payload =
                    jsonBody.toByteArray(StandardCharsets.UTF_8)
                val conn = (URL(url).openConnection() as
                        HttpURLConnection).apply {
                    requestMethod = "POST"
                    connectTimeout = 15_000
                    readTimeout = 15_000
                    doInput = true
                    doOutput = true
                    setRequestProperty(
                        "Content-Type",
                        "application/json; charset=utf-8"
                    )
                    setRequestProperty(
                        "Accept",
                        "application/json"
                    )
                    headers.forEach { (k, v) ->
                        setRequestProperty(k, v)
                    }
                }

                conn.outputStream.use {
                    it.write(payload)
                }
                conn.useAndRead()
            }
        }

    private suspend inline fun dnsAwareRequest(
        url: String,
        block: () -> Result
    ): Result {
        val host = runCatching { URL(url).host }.getOrNull().orEmpty()
        val resolvedHost = host.ifBlank { "target host" }
        var last: UnknownHostException? = null

        repeat(3) { attempt ->
            try {
                return block()
            } catch (e: UnknownHostException) {
                last = e
                if (attempt < 2) {
                    delay((attempt + 1) * 300L)
                }
            }
        }

        throw IllegalStateException(
            "DNS failed for $resolvedHost after retries. Check Private DNS / VPN / network.",
            last
        )
    }

    private fun HttpURLConnection.useAndRead():
            Result {
        return try {
            val status = responseCode
            val stream = if (status in 200..299)
                inputStream else errorStream
            val body =
                stream?.bufferedReader(StandardCharsets.UTF_8)?.use {
                    it.readText()
                }.orEmpty()
            Result(status = status, body = body)
        } finally {
            disconnect()
        }
    }
}
