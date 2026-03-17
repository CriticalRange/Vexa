package com.critical.vexaemulator.auth

import org.json.JSONObject

data class TokenResponse(
    val accessToken: String?,
    val refreshToken: String?,
    val idToken: String?,
    val tokenType: String?,
    val expiresIn: Long?,
    val scope: String?,
)

data class GameProfile(
    val uuid: String,
    val name: String,
)

data class GameTokens(
    val identityToken: String,
    val sessionToken: String,
)

data class AuthState(
    var oauthAccessToken: String? = null,
    var oauthAccessTokenExpiresAtEpochSec: Long? = null,
    var oauthRefreshToken: String? = null,
    var oauthIdToken: String? = null,
    var oauthScope: String? = null,
    var gameIdentityToken: String? = null,
    var gameSessionToken: String? = null,
    var gameSessionExpiresAtEpochSec: Long? = null,
    var gameProfileUuid: String? = null,
    var gameProfileName: String? = null,
    var lastUpdatedEpochMs: Long? = null,
) {
    val hasOauthRefreshToken: Boolean get() = !oauthRefreshToken.isNullOrBlank()
    val hasGameTokens: Boolean get() = !gameIdentityToken.isNullOrBlank() && !gameSessionToken.isNullOrBlank()

    fun toJson(): JSONObject = JSONObject().apply {
        putIfNotNull(
            "oauthAccessToken",
            oauthAccessToken
        )
        putIfNotNull(
            "oauthAccessTokenExpiresAtEpochSec",
            oauthAccessTokenExpiresAtEpochSec
        )
        putIfNotNull(
            "oauthRefreshToken",
            oauthRefreshToken
        )
        putIfNotNull("oauthIdToken", oauthIdToken)
        putIfNotNull("oauthScope", oauthScope)
        putIfNotNull(
            "gameIdentityToken",
            gameIdentityToken
        )
        putIfNotNull(
            "gameSessionToken",
            gameSessionToken
        )
        putIfNotNull(
            "gameSessionExpiresAtEpochSec",
            gameSessionExpiresAtEpochSec
        )
        putIfNotNull(
            "gameProfileUuid",
            gameProfileUuid
        )
        putIfNotNull(
            "gameProfileName",
            gameProfileName
        )
        putIfNotNull(
            "lastUpdatedEpochMs",
            lastUpdatedEpochMs
        )
    }

    companion object {
        fun fromJson(obj: JSONObject): AuthState {
            return AuthState(
                oauthAccessToken =
                    obj.optNullableString("oauthAccessToken"),
                oauthAccessTokenExpiresAtEpochSec =
                    obj.optNullableLong("oauthAccessTokenExpiresAtEpochSec"),
                oauthRefreshToken =
                    obj.optNullableString("oauthRefreshToken"),
                oauthIdToken =
                    obj.optNullableString("oauthIdToken"),
                oauthScope =
                    obj.optNullableString("oauthScope"),
                gameIdentityToken =
                    obj.optNullableString("gameIdentityToken"),
                gameSessionToken =
                    obj.optNullableString("gameSessionToken"),
                gameSessionExpiresAtEpochSec =
                    obj.optNullableLong("gameSessionExpiresAtEpochSec"),
                gameProfileUuid =
                    obj.optNullableString("gameProfileUuid"),
                gameProfileName =
                    obj.optNullableString("gameProfileName"),
                lastUpdatedEpochMs =
                    obj.optNullableLong("lastUpdatedEpochMs"),
            )
        }
    }
}

private fun JSONObject.putIfNotNull(
    key: String,
    value: Any?
) {
    if (value != null) put(key, value)
}

private fun JSONObject.optNullableString(
    key:
    String
): String? {
    val value = optString(key, "")
    return value.takeIf { it.isNotBlank() }
}

private fun JSONObject.optNullableLong(key: String):
        Long? {
    if (!has(key)) return null
    val value = optLong(key, Long.MIN_VALUE)
    return if (value == Long.MIN_VALUE) null else
        value
}