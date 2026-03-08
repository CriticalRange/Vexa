package com.critical.vexaemulator

import android.content.Intent
import android.os.Bundle
import android.os.Environment
import android.provider.Settings
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
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.core.net.toUri
import com.critical.vexaemulator.logging.LogStore
import com.critical.vexaemulator.ui.theme.VexaEmulatorTheme

class MainActivity : ComponentActivity() {

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

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val hasAllFilesAccess = hasAllFilesAccess()
        if (!hasAllFilesAccess) {
            requestAllFilesAccess()
        }
        enableEdgeToEdge()
        setContent {
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
                            onClick = {},
                            contentPadding = PaddingValues(horizontal = 48.dp, vertical = 12.dp)
                        ) {
                            Text("Login")
                        }
                        Spacer(modifier = Modifier.height(12.dp))
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