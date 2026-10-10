package com.critical.vexaemulator

import android.content.Intent
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Checkbox
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.critical.vexaemulator.guest.GuestCatalog
import com.critical.vexaemulator.logging.LogCategory
import com.critical.vexaemulator.logging.LogLevel
import com.critical.vexaemulator.logging.LogStore
import com.critical.vexaemulator.logging.VexaLogger
import com.critical.vexaemulator.runtime.RootLogCollector
import com.critical.vexaemulator.ui.theme.VexaEmulatorTheme
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContent {
            VexaEmulatorTheme {
                LauncherScreen(
                    onRun = { programId ->
                        LogStore.clear()
                        startActivity(
                            Intent(this, GameActivity::class.java)
                                .putExtra(GameActivity.EXTRA_PROGRAM_ID, programId)
                        )
                    },
                    onQuit = { finish() },
                )
            }
        }
    }
}

@Composable
private fun LauncherScreen(
    onRun: (String) -> Unit,
    onQuit: () -> Unit,
) {
    val programs = GuestCatalog.programs
    var selectedId by remember { mutableStateOf(programs.first().id) }
    var menuOpen by remember { mutableStateOf(false) }
    val selected = programs.first { it.id == selectedId }
    val entries by LogStore.entries.collectAsState()

    val context = LocalContext.current
    var rootLoggingEnabled by remember {
        mutableStateOf(RootLogCollector.isEnabled(context.filesDir))
    }
    // null = probing, "" = no usable su, otherwise the su path that worked
    var rootSuPath by remember { mutableStateOf<String?>(null) }
    LaunchedEffect(Unit) {
        rootSuPath = withContext(Dispatchers.IO) { RootLogCollector.probeSu() } ?: ""
    }

    Scaffold(
        modifier = Modifier.fillMaxSize(),
        containerColor = Color(0xFF121212),
        contentColor = Color.White,
    ) { innerPadding ->
        Column(
            modifier = Modifier
                .padding(innerPadding)
                .padding(20.dp)
                .fillMaxSize(),
            verticalArrangement = Arrangement.Top,
        ) {
            Text("VEXA", color = Color.White, fontSize = 28.sp)
            Text(
                "Run a Linux x86_64 program",
                color = Color(0xFFB0B0B0),
                fontSize = 14.sp,
            )
            Spacer(modifier = Modifier.height(20.dp))
            Text("Program", color = Color(0xFFD0D0D0), fontSize = 12.sp)
            Spacer(modifier = Modifier.height(6.dp))
            Box(modifier = Modifier.fillMaxWidth()) {
                Button(
                    onClick = { menuOpen = true },
                    modifier = Modifier.fillMaxWidth(),
                    contentPadding = PaddingValues(horizontal = 16.dp, vertical = 14.dp),
                ) {
                    Text(selected.title)
                }
                DropdownMenu(
                    expanded = menuOpen,
                    onDismissRequest = { menuOpen = false },
                ) {
                    programs.forEach { program ->
                        DropdownMenuItem(
                            text = { Text(program.title) },
                            onClick = {
                                selectedId = program.id
                                menuOpen = false
                            },
                        )
                    }
                }
            }
            Spacer(modifier = Modifier.height(8.dp))
            Text(selected.summary, color = Color(0xFF9A9A9A), fontSize = 13.sp)
            Spacer(modifier = Modifier.height(16.dp))
            Button(
                onClick = { onRun(selected.id) },
                modifier = Modifier.fillMaxWidth(),
                contentPadding = PaddingValues(vertical = 14.dp),
            ) {
                Text("Run")
            }
            Spacer(modifier = Modifier.height(12.dp))
            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Checkbox(
                    checked = rootLoggingEnabled,
                    onCheckedChange = { checked ->
                        rootLoggingEnabled = checked
                        val wrote = RootLogCollector.setEnabled(context.filesDir, checked)
                        VexaLogger.log(
                            LogLevel.INFO,
                            LogCategory.CONFIG,
                            "Root logging ${if (checked) "enabled" else "disabled"}",
                            mapOf(
                                "source" to "launcher",
                                "flagWritten" to wrote.toString(),
                            ),
                        )
                    },
                )
                Column {
                    Text("Root logging", color = Color.White, fontSize = 14.sp)
                    Text(
                        text = when {
                            rootSuPath == null -> "Checking for su…"
                            rootSuPath!!.isEmpty() ->
                                "No root for this app — grant in KernelSU Manager (safe no-op)"
                            else -> "su: $rootSuPath — dmesg/proc snapshots go to artifacts"
                        },
                        color = if (rootSuPath.isNullOrEmpty()) {
                            Color(0xFF8A8A8A)
                        } else {
                            Color(0xFF81C784)
                        },
                        fontSize = 11.sp,
                    )
                }
            }
            Spacer(modifier = Modifier.height(8.dp))
            Text("Logs", color = Color.White)
            Spacer(modifier = Modifier.height(8.dp))
            LazyColumn(
                modifier = Modifier
                    .fillMaxWidth()
                    .weight(1f)
                    .border(1.dp, Color(0xFF333333))
                    .background(Color(0xFF111111))
                    .padding(12.dp),
            ) {
                if (entries.isEmpty()) {
                    item {
                        Text("No logs yet.", color = Color(0xFF8A8A8A), fontFamily = FontFamily.Monospace)
                    }
                } else {
                    items(entries.takeLast(16), key = { it.id }) { entry ->
                        Text(
                            text = "${entry.level} ${entry.category}: ${entry.message}",
                            color = if (entry.level.name == "ERROR" || entry.level.name == "FATAL") {
                                Color(0xFFFF8A80)
                            } else {
                                Color(0xFFD7D7D7)
                            },
                            fontFamily = FontFamily.Monospace,
                            fontSize = 12.sp,
                        )
                    }
                }
            }
            Spacer(modifier = Modifier.height(12.dp))
            Button(
                onClick = onQuit,
                modifier = Modifier.fillMaxWidth(),
                colors = ButtonDefaults.buttonColors(
                    containerColor = Color(0xFFD32F2F),
                    contentColor = Color.White,
                ),
            ) {
                Text("Quit")
            }
        }
    }
}
