package com.critical.vexaemulator

import android.content.Intent
import android.os.Bundle
import android.os.Environment
import android.provider.Settings
import android.util.Log
import androidx.activity.ComponentActivity
import androidx.activity.compose.LocalActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.core.net.toUri
import com.critical.vexaemulator.auth.AuthManager
import com.critical.vexaemulator.logging.LogStore
import com.critical.vexaemulator.ui.theme.VexaEmulatorTheme
import kotlinx.coroutines.launch

class MainActivity : ComponentActivity() {

    private fun isOAuthDoneIntent(i: Intent?): Boolean {
        val d = i?.data ?: return false
        return i.action == Intent.ACTION_VIEW &&
                d.scheme == "vexa" &&
                d.host == "oauth" &&
                d.path == "/done"
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        setIntent(intent)
        if (isOAuthDoneIntent(intent)) {
            Log.i("VEXA-AUTH", "OAuth done deep link received (onNewIntent)")
        }
    }

    private fun hasAllFilesAccess(): Boolean {
        return Environment.isExternalStorageManager()
    }

    private fun requestAllFilesAccess() {
        try {
            val intent = Intent( // check for all files access
                Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION, "package:$packageName".toUri()
            )
            startActivity(intent)
        } catch (_: Exception) {
            try {
                val intent = Intent( // check for app all files access
                    Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,
                    "package:$packageName".toUri()
                )
                startActivity(intent)
            } catch (_: Exception) {
                val intent = Intent( // check for application details
                    Settings.ACTION_APPLICATION_DETAILS_SETTINGS
                )
                    .apply {
                        data = "package:$packageName".toUri()
                    }
                startActivity(intent)
            }
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        if (isOAuthDoneIntent(intent)) {
            Log.i("VEXA-AUTH", "OAuth done deep link received (onCreate)")
        }
        val hasAllFilesAccess = hasAllFilesAccess()
        if (!hasAllFilesAccess) {
            requestAllFilesAccess()
        }
        enableEdgeToEdge()
        setContent {
            val activity = LocalActivity.current
            val authManager = remember(activity) {
                activity?.applicationContext?.let {
                    AuthManager(it)
                }
            }
            var authState by remember(authManager) {
                mutableStateOf(authManager?.load())
            }
            var authBusy by remember { mutableStateOf(false) }
            val scope = rememberCoroutineScope()

            val authStatus = when {
                authState?.hasGameTokens == true -> "Auth: ready"
                authState?.hasOauthRefreshToken == true -> "Auth: logged in (needs game tokens"
                else -> "Auth: not logged in"
            }

            val lifecycleOwner = androidx.lifecycle.compose.LocalLifecycleOwner.current
            androidx.compose.runtime.DisposableEffect(lifecycleOwner, authManager) {
                val observer = androidx.lifecycle.LifecycleEventObserver { _, event ->
                    if (event == androidx.lifecycle.Lifecycle.Event.ON_RESUME) {
                        authState = authManager?.load()
                    }
                }
                lifecycleOwner.lifecycle.addObserver(observer)
                onDispose {
                    lifecycleOwner.lifecycle.removeObserver(observer)
                }
            }
            val lastFatal = LogStore.lastFatal.collectAsState().value
            VexaEmulatorTheme {
                Scaffold(modifier = Modifier.fillMaxSize()) { innerPadding ->
                    val activity = LocalActivity.current
                    Column(
                        modifier = Modifier
                            .padding(innerPadding)
                            .padding(16.dp)
                            .fillMaxSize(),
                        verticalArrangement = Arrangement.Top
                    ) {
                        Button(
                            onClick = {
                                LogStore.clear()
                                activity?.startActivity(
                                    Intent(
                                        activity,
                                        GameActivity::class.java
                                    )
                                )
                            },
                            modifier = Modifier.fillMaxWidth(),
                            contentPadding = PaddingValues(horizontal = 24.dp, vertical = 12.dp)
                        ) {
                            Text("Play")
                        }
                        Spacer(modifier = Modifier.height(12.dp))
                        Button(
                            onClick = {
                                val hostActivity = activity ?: return@Button
                                val manager = authManager ?: return@Button
                                if (authBusy) return@Button

                                scope.launch {
                                    authBusy = true
                                    try {
                                        if (
                                            authState?.hasOauthRefreshToken == true
                                        ) {
                                            authState = manager.refreshGameSession()
                                        } else {
                                            authState = manager.login(hostActivity)

                                            authState = runCatching {
                                                manager.ensureGameTokens()
                                            }.getOrElse { err ->
                                                Log.w(
                                                    "VEXA-AUTH",
                                                    "OAuth succeeded but game token bootstrap failed",
                                                    err
                                                )
                                                manager.load()
                                            }
                                        }

                                        Log.i("VEXA-AUTH", "Auth flow completed")
                                    } catch (t: Throwable) {
                                        authState = manager.load()

                                        val summary = buildString {
                                            append(t.javaClass.name)
                                            if (!t.message.isNullOrBlank()) {
                                                append(": ").append(t.message)
                                            }

                                            var cause = t.cause
                                            var depth = 0
                                            while (cause != null && depth < 4) {
                                                append(" | cause=").append(cause.javaClass.name)
                                                if (!cause.message.isNullOrBlank()) {
                                                    append(": ").append(cause.message)
                                                }
                                                cause = cause.cause
                                                depth++
                                            }
                                        }

                                        Log.e("VEXA-AUTH", "Auth flow failed -> $summary")
                                        Log.e(
                                            "VEXA-AUTH",
                                            "Auth flow failed stack:\n${t.stackTraceToString()}"
                                        )
                                        // TODO: Add actual login error (with toast)
                                    } finally {
                                        authBusy = false
                                    }
                                }
                            },
                            enabled = !authBusy,
                            contentPadding = PaddingValues(horizontal = 48.dp, vertical = 12.dp)
                        ) {
                            Text(
                                when {
                                    authBusy -> "Working..."
                                    authState?.hasOauthRefreshToken == true -> "Refresh Session"
                                    else -> "Login"
                                }
                            )
                        }
                        Spacer(modifier = Modifier.height(12.dp))
                        if (authState?.hasOauthRefreshToken == true ||
                            authState?.hasGameTokens == true
                        ) {
                            Spacer(modifier = Modifier.height(8.dp))
                            Button(
                                onClick = {
                                    val manager = authManager ?: return@Button
                                    if (authBusy) return@Button

                                    manager.logout()
                                    authState = manager.load() // should become null
                                },
                                enabled = !authBusy,
                                modifier = Modifier.fillMaxWidth(),
                                colors = ButtonDefaults.buttonColors(
                                    containerColor = Color(0xFF8B1E1E),
                                    contentColor = Color.White
                                ),
                                contentPadding = PaddingValues(
                                    horizontal =
                                        24.dp, vertical = 12.dp
                                )
                            ) {
                                Text("Logout")
                            }
                        }
                        Text(
                            text = authStatus,
                            color = Color.LightGray
                        )
                        Column(
                            modifier = Modifier
                                .fillMaxWidth()
                                .weight(1f)
                                .border(1.dp, Color.Gray)
                                .background(Color(0xFF111111))
                                .padding(12.dp)
                        ) {
                            Text(
                                text = "Logs",
                                color = Color.White
                            )
                            Spacer(modifier = Modifier.height(8.dp))
                            Text(
                                text = if (lastFatal == null) {
                                    "No logs yet :D"
                                } else {
                                    "Game has crashed: ${lastFatal.message}"
                                },
                                color = Color.LightGray
                            )
                        }
                        Spacer(modifier = Modifier.height(8.dp))
                        Button(
                            onClick = { activity?.finish() },
                            modifier = Modifier.fillMaxWidth(),
                            colors = ButtonDefaults.buttonColors(
                                containerColor = Color(0xFFD32F2F),
                                contentColor = Color.White
                            ),
                            contentPadding = PaddingValues(horizontal = 24.dp, vertical = 12.dp)
                        ) {
                            Text("Quit")
                        }
                    }
                }
            }
        }
    }
}

@Composable
fun Greeting(name: String, modifier: Modifier = Modifier) {
    Text(
        text = "Hello $name!",
        modifier = modifier
    )
}

@Preview(showBackground = true)
@Composable
fun GreetingPreview() {
    VexaEmulatorTheme {
        Greeting("Android")
    }
}