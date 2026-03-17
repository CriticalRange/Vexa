package com.critical.vexaemulator.auth

import android.content.Context
import android.security.keystore.KeyGenParameterSpec
import android.security.keystore.KeyProperties
import org.json.JSONObject
import java.io.File
import java.nio.charset.StandardCharsets
import java.security.KeyStore
import javax.crypto.Cipher
import javax.crypto.KeyGenerator
import javax.crypto.SecretKey
import javax.crypto.spec.GCMParameterSpec

class SecureAuthStore(
    private val context: Context
) {
    private companion object {
        private const val KEY_ALIAS = "vexa_auth_aes_gcm_v1"
        private const val FILE_NAME = "auth_state.dat"
        private const val VERSION = 1
    }

    private fun stateFile(): File {
        val dir = context.noBackupFilesDir ?: context.filesDir
        return File(dir, FILE_NAME)
    }

    fun load(): AuthState? {
        val f = stateFile()
        if (!f.exists()) return null

        return runCatching {
            val wrapper = JSONObject(f.readText(StandardCharsets.UTF_8))
            if (wrapper.optInt("v", -1) != VERSION) return null

            val iv = java.util.Base64.getDecoder().decode(wrapper.getString("iv"))
            val ct = java.util.Base64.getDecoder().decode(wrapper.getString("ct"))

            val cipher = Cipher.getInstance("AES/GCM/NoPadding")
            cipher.init(Cipher.DECRYPT_MODE, getOrCreateKey(), GCMParameterSpec(128, iv))
            val pt = cipher.doFinal(ct)
            val json = String(
                pt,
                StandardCharsets.UTF_8
            )
            AuthState.fromJson(JSONObject(json))
        }.getOrNull()
    }

    fun save(state: AuthState) {
        state.lastUpdatedEpochMs = System.currentTimeMillis()

        val plainText = state.toJson().toString().toByteArray(StandardCharsets.UTF_8)

        val cipher = Cipher.getInstance("AES/GCM/NoPadding")
        cipher.init(Cipher.ENCRYPT_MODE, getOrCreateKey())
        val iv = cipher.iv
        val ct = cipher.doFinal(plainText)

        val wrapper = JSONObject().apply {
            put("v", VERSION)
            put("iv", java.util.Base64.getEncoder().encodeToString(iv))
            put("ct", java.util.Base64.getEncoder().encodeToString(ct))
        }

        val f = stateFile()
        val tmp = File(f.parentFile, "${f.name}.tmp")
        tmp.writeText(wrapper.toString(), StandardCharsets.UTF_8)
        tmp.renameTo(f)
    }

    fun delete() {
        stateFile().takeIf {
            it.exists()
        }?.delete()
    }

    private fun getOrCreateKey(): SecretKey {
        val ks = KeyStore.getInstance("AndroidKeyStore").apply {
            load(null)
        }
        (ks.getKey(KEY_ALIAS, null) as? SecretKey)?.let { return it }

        val generator = KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES, "AndroidKeyStore")
        val spec = KeyGenParameterSpec.Builder(
            KEY_ALIAS,
            KeyProperties.PURPOSE_ENCRYPT or KeyProperties.PURPOSE_DECRYPT
        )
            .setBlockModes(KeyProperties.BLOCK_MODE_GCM)
            .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
            .setKeySize(256)
            .setRandomizedEncryptionRequired(true)
            .build()

        generator.init(spec)
        return generator.generateKey()
    }
}