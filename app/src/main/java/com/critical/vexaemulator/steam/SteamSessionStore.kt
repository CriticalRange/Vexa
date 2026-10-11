package com.critical.vexaemulator.steam

import android.content.Context
import android.security.keystore.KeyGenParameterSpec
import android.security.keystore.KeyProperties
import com.critical.vexaemulator.logging.LogCategory
import com.critical.vexaemulator.logging.LogLevel
import com.critical.vexaemulator.logging.VexaLogger
import org.json.JSONObject
import java.io.File
import java.io.FileOutputStream
import java.security.KeyStore
import javax.crypto.Cipher
import javax.crypto.KeyGenerator
import javax.crypto.SecretKey
import javax.crypto.spec.GCMParameterSpec

data class StoredSteamSession(
    val accountName: String,
    val steamId: String,
    val refreshToken: String,
    val guardData: String?,
)

// Refresh tokens stay in an app-private file encrypted with a keystore key.
// The password is never written. Backup rules exclude this directory.
object SteamSessionStore {
    private const val keystore = "AndroidKeyStore"
    private const val alias = "vexa-steam-session"
    private const val transformation = "AES/GCM/NoPadding"
    private const val ivLength = 12
    private const val tagBits = 128

    fun load(context: Context): StoredSteamSession? {
        val file = sessionFile(context)
        if (!file.isFile) return null
        // An older build cleared the owner read bit. Put it back before reading.
        file.setReadable(true, true)
        val packed = try {
            file.readBytes()
        } catch (error: Exception) {
            noteFailure("Steam session file could not be read", error)
            return null
        }
        return try {
            decode(decrypt(packed))
        } catch (error: Exception) {
            noteFailure("Steam session file could not be decrypted", error)
            file.delete()
            null
        }
    }

    fun save(context: Context, session: StoredSteamSession) {
        val file = sessionFile(context)
        file.parentFile?.mkdirs()
        val cipher = Cipher.getInstance(transformation)
        cipher.init(Cipher.ENCRYPT_MODE, secretKey())
        val iv = cipher.iv
        check(iv.size == ivLength)
        val body = cipher.doFinal(encode(session).toByteArray(Charsets.UTF_8))
        FileOutputStream(file).use { out ->
            out.write(iv)
            out.write(body)
            out.channel.force(true)
        }
        // Owner can read it. Other apps already cannot see filesDir.
        file.setReadable(true, true)
        file.setWritable(true, true)
        val stored = decode(decrypt(file.readBytes()))
        check(stored.accountName == session.accountName && stored.refreshToken == session.refreshToken) {
            "Steam session could not be stored"
        }
        VexaLogger.log(
            LogLevel.INFO,
            LogCategory.CONFIG,
            "Steam session stored",
            mapOf("steamId" to session.steamId),
        )
    }

    fun clear(context: Context) {
        sessionFile(context).delete()
    }

    private fun noteFailure(message: String, error: Exception) {
        VexaLogger.log(
            LogLevel.ERROR,
            LogCategory.FAILURE,
            message,
            mapOf("error" to error.javaClass.simpleName),
        )
    }

    private fun sessionFile(context: Context): File =
        File(context.filesDir, "steam/session.bin")

    private fun secretKey(): SecretKey {
        val store = KeyStore.getInstance(keystore).apply { load(null) }
        (store.getKey(alias, null) as? SecretKey)?.let { return it }
        val generator = KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES, keystore)
        generator.init(
            KeyGenParameterSpec.Builder(
                alias,
                KeyProperties.PURPOSE_ENCRYPT or KeyProperties.PURPOSE_DECRYPT,
            )
                .setBlockModes(KeyProperties.BLOCK_MODE_GCM)
                .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
                .setKeySize(256)
                .build(),
        )
        return generator.generateKey()
    }

    private fun decrypt(packed: ByteArray): String {
        require(packed.size > ivLength)
        val cipher = Cipher.getInstance(transformation)
        cipher.init(
            Cipher.DECRYPT_MODE,
            secretKey(),
            GCMParameterSpec(tagBits, packed.copyOfRange(0, ivLength)),
        )
        return cipher.doFinal(packed.copyOfRange(ivLength, packed.size)).toString(Charsets.UTF_8)
    }

    private fun encode(session: StoredSteamSession): String = JSONObject()
        .put("account", session.accountName)
        .put("steamId", session.steamId)
        .put("refresh", session.refreshToken)
        .put("guard", session.guardData ?: "")
        .toString()

    private fun decode(json: String): StoredSteamSession {
        val objectJson = JSONObject(json)
        val guard = objectJson.optString("guard")
        return StoredSteamSession(
            accountName = objectJson.getString("account"),
            steamId = objectJson.getString("steamId"),
            refreshToken = objectJson.getString("refresh"),
            guardData = guard.ifEmpty { null },
        )
    }
}
