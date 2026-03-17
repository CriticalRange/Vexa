package com.critical.vexaemulator.auth

import org.json.JSONArray
import org.json.JSONObject
import org.json.JSONTokener

class HytaleSessionsClient {
    companion object {
        private const val PROFILES_URL =
            "https://account-data.hytale.com/my-account/get-launcher-data"
        private const val SESSION_NEW_URL =
            "https://sessions.hytale.com/game-session/new"
        private const val SESSION_REFRESH_URL =
            "https://sessions.hytale.com/game-session/refresh"
    }

    private fun bearerHeaders(token: String):
            Map<String, String> = mapOf(
        "Authorization" to "Bearer $token",
        "Accept" to "application/json",
        "User-Agent" to "Go-http-client/1.1",
    )

    suspend fun getProfiles(
        oauthAccessToken:
        String
    ): List<GameProfile> {
        val res = AuthHttp.get(
            PROFILES_URL, headers
            = bearerHeaders(oauthAccessToken)
        )
        if (res.status !in 200..299) {
            throw IllegalStateException("get-launcher-data failed: ${res.status}${res.body.take(300)}")
        }

        val payload =
            JSONTokener(res.body).nextValue()
        val out = mutableListOf<GameProfile>()

        fun parseProfileObject(obj: JSONObject) {
            val uuid = obj.optString("uuid", "")
            if (uuid.isBlank()) return
            val name = obj.optString(
                "name",
                obj.optString("username", uuid)
            ).ifBlank { uuid }
            out += GameProfile(
                uuid = uuid, name =
                    name
            )
        }

        when (payload) {
            is JSONObject -> {
                when {
                    payload.has("profiles") -> {
                        val arr =
                            payload.optJSONArray("profiles") ?: JSONArray()
                        for (i in 0 until
                                arr.length())
                            (arr.optJSONObject(i))?.let(::parseProfileObject)
                    }

                    payload.has("gameProfiles") -> {
                        val arr =
                            payload.optJSONArray("gameProfiles") ?: JSONArray()
                        for (i in 0 until
                                arr.length())
                            (arr.optJSONObject(i))?.let(::parseProfileObject)
                    }

                    else ->
                        parseProfileObject(payload)
                }
            }

            is JSONArray -> {
                for (i in 0 until payload.length())
                    (payload.optJSONObject(i))?.let(
                        ::parseProfileObject
                    )
            }
        }

        return out
    }

    suspend fun acquireGameTokens(
        oauthAccessToken:
        String, profileUuid: String
    ): GameTokens {
        val body = JSONObject().put(
            "uuid",
            profileUuid
        ).toString()
        val res = AuthHttp.postJson(
            url = SESSION_NEW_URL,
            jsonBody = body,
            headers =
                bearerHeaders(oauthAccessToken),
        )
        if (res.status !in 200..299) {
            throw IllegalStateException("game-session/new failed: ${res.status}${res.body.take(300)}")
        }

        val parsed = tryParseGameTokens(res.body)
        if (parsed == null) {
            throw IllegalStateException("game-session/new returned unparseable token payload")
        }
        return parsed
    }

    suspend fun refreshGameSession(
        sessionToken:
        String
    ): GameTokens {
        val res = AuthHttp.postJson(
            url = SESSION_REFRESH_URL,
            jsonBody = "{}",
            headers = bearerHeaders(sessionToken),
        )
        if (res.status !in 200..299) {
            throw IllegalStateException(
                "game-session/refresh failed: ${res.status}${
                    res.body.take(
                        300
                    )
                }"
            )
        }

        val parsed = tryParseGameTokens(res.body)
        if (parsed == null) {
            throw IllegalStateException("game-session/refresh returned unparseable token payload")
        }
        return parsed
    }

    private fun tryParseGameTokens(body: String):
            GameTokens? {
        val obj = JSONTokener(body).nextValue() as?
                JSONObject ?: return null

        val identity =
            obj.optString(
                "identityToken",
                obj.optString("identity_token", "")
            )
        val session = obj.optString(
            "sessionToken",
            obj.optString("session_token", "")
        )
        if (identity.isBlank() || session.isBlank())
            return null

        return GameTokens(
            identityToken = identity,
            sessionToken = session
        )
    }
}