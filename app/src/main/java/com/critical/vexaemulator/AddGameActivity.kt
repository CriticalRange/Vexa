package com.critical.vexaemulator

import android.graphics.Bitmap
import android.net.Uri
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.OutlinedTextFieldDefaults
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import androidx.compose.foundation.Image
import com.critical.vexaemulator.guest.CustomGuestStore
import com.critical.vexaemulator.guest.ElfArtwork
import com.critical.vexaemulator.guest.ElfProbe
import com.critical.vexaemulator.guest.GuestCatalog
import com.critical.vexaemulator.guest.GuestProgram
import com.critical.vexaemulator.ui.SectionLabel
import com.critical.vexaemulator.ui.VexaButton
import com.critical.vexaemulator.ui.VexaPrefs
import com.critical.vexaemulator.ui.theme.PillShape
import com.critical.vexaemulator.ui.theme.VexaEmulatorTheme
import com.critical.vexaemulator.ui.theme.VexaTheme
import com.critical.vexaemulator.ui.theme.resolveDark
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.File

class AddGameActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        VexaPrefs.init(this)
        enableEdgeToEdge()
        setContent {
            VexaEmulatorTheme(darkTheme = VexaPrefs.themeMode.resolveDark()) {
                AddGameScreen(onDone = { finish() })
            }
        }
    }
}

@OptIn(ExperimentalLayoutApi::class)
@Composable
private fun AddGameScreen(onDone: () -> Unit) {
    val c = VexaTheme.colors
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    var error by remember { mutableStateOf<String?>(null) }
    var busy by remember { mutableStateOf(false) }
    var staged by remember { mutableStateOf<File?>(null) }
    var probe by remember { mutableStateOf<ElfProbe?>(null) }
    var title by remember { mutableStateOf("") }
    var summary by remember { mutableStateOf("") }
    var fileName by remember { mutableStateOf("") }
    var arch by remember { mutableStateOf("") }
    var appId by remember { mutableStateOf("") }
    var envText by remember { mutableStateOf("DISPLAY=:0") }
    var icon by remember { mutableStateOf<Bitmap?>(null) }

    val picker = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri: Uri? ->
        if (uri == null) return@rememberLauncherForActivityResult
        scope.launch {
            busy = true
            error = null
            val result = withContext(Dispatchers.IO) { stageElf(context, uri) }
            busy = false
            result.fold(
                onSuccess = { stagedFile ->
                    val read = withContext(Dispatchers.IO) { ElfArtwork.probe(stagedFile) }
                    staged = stagedFile
                    probe = read
                    fileName = stagedFile.name
                    if (!read.accepted) {
                        error = read.message
                    } else {
                        title = read.title
                        summary = read.summary
                        arch = read.arch.orEmpty()
                        appId = read.steamAppId.orEmpty()
                        icon = read.icon
                        if (read.steamAppId != null && !envText.lines().any { it.startsWith("SteamAppId=") }) {
                            envText = envText.trimEnd() + "\nSteamAppId=${read.steamAppId}"
                        }
                    }
                },
                onFailure = { error = it.message ?: "Could not read that file." },
            )
        }
    }

    Column(
        Modifier
            .fillMaxSize()
            .background(c.bgBase)
            .verticalScroll(rememberScrollState())
            .statusBarsPadding()
            .navigationBarsPadding()
            .padding(horizontal = 20.dp),
    ) {
        Spacer(Modifier.height(14.dp))
        SubscreenHeader(title = "Add game", onBack = onDone)
        Spacer(Modifier.height(8.dp))
        // TODO: Proton/Windows import is later. This form only accepts a Linux x86_64 ELF.
        Text(
            "Pick a Linux x86_64 ELF. Name, summary, and icon are filled from the binary when they can be read.",
            color = c.textSecondary,
            style = androidx.compose.material3.MaterialTheme.typography.bodyMedium,
        )
        Spacer(Modifier.height(14.dp))
        VexaButton(
            text = if (busy) "Reading ELF" else "Choose ELF",
            onClick = { if (!busy) picker.launch(arrayOf("*/*")) },
            enabled = !busy,
            modifier = Modifier.fillMaxWidth(),
        )
        error?.let {
            Spacer(Modifier.height(10.dp))
            Text(it, color = c.error, style = androidx.compose.material3.MaterialTheme.typography.bodyMedium)
        }
        if (probe?.accepted == true) {
            Spacer(Modifier.height(16.dp))
            icon?.let { bmp ->
                Image(
                    bitmap = bmp.asImageBitmap(),
                    contentDescription = title,
                    contentScale = ContentScale.Fit,
                    modifier = Modifier
                        .height(72.dp)
                        .clip(RoundedCornerShape(12.dp)),
                )
                Spacer(Modifier.height(10.dp))
            }
            Field("Title", title) { title = it }
            Field("Summary", summary) { summary = it }
            Field("File name", fileName) { fileName = it }
            Field("Arch", arch) { arch = it }
            Field("SteamAppId", appId) { appId = it }
            Spacer(Modifier.height(8.dp))
            SectionLabel(text = "Environment")
            Spacer(Modifier.height(8.dp))
            Field("KEY=VALUE per line", envText, singleLine = false) { envText = it }
            Spacer(Modifier.height(8.dp))
            Text("Suggestions", color = c.textTertiary, style = androidx.compose.material3.MaterialTheme.typography.labelMedium)
            Spacer(Modifier.height(8.dp))
            FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                GuestCatalog.envSuggestions().forEach { line ->
                    val present = envText.lines().any { it.trim() == line }
                    Text(
                        text = line,
                        color = if (present) c.onAccent else c.textSecondary,
                        style = androidx.compose.material3.MaterialTheme.typography.labelSmall,
                        modifier = Modifier
                            .clip(PillShape)
                            .background(if (present) c.plasma else c.surface2)
                            .border(1.dp, c.hairline, PillShape)
                            .clickable {
                                if (!envText.lines().any { it.trim() == line }) {
                                    envText = (envText.trimEnd() + "\n" + line).trim()
                                }
                            }
                            .padding(horizontal = 10.dp, vertical = 6.dp),
                    )
                }
            }
            Spacer(Modifier.height(16.dp))
            VexaButton(
                text = "Save game",
                enabled = title.isNotBlank() && fileName.isNotBlank() && !busy,
                onClick = {
                    val source = staged ?: return@VexaButton
                    scope.launch {
                        busy = true
                        val saved = withContext(Dispatchers.IO) {
                            persist(context, source, title, summary, fileName, arch, appId, envText, icon)
                        }
                        busy = false
                        if (saved) onDone() else error = "Could not save that game."
                    }
                },
                modifier = Modifier.fillMaxWidth(),
            )
        }
        Spacer(Modifier.height(24.dp))
    }
}

@Composable
private fun Field(label: String, value: String, singleLine: Boolean = true, onChange: (String) -> Unit) {
    val c = VexaTheme.colors
    OutlinedTextField(
        value = value,
        onValueChange = onChange,
        label = { Text(label) },
        singleLine = singleLine,
        minLines = if (singleLine) 1 else 4,
        modifier = Modifier.fillMaxWidth().padding(bottom = 8.dp),
        colors = OutlinedTextFieldDefaults.colors(
            focusedTextColor = c.textPrimary,
            unfocusedTextColor = c.textPrimary,
            focusedBorderColor = c.plasma,
            unfocusedBorderColor = c.hairline,
            focusedLabelColor = c.plasma,
            unfocusedLabelColor = c.textSecondary,
            cursorColor = c.plasma,
        ),
    )
}

private fun stageElf(context: android.content.Context, uri: Uri): Result<File> = runCatching {
    val name = queryName(context, uri).ifBlank { "game.elf" }.substringAfterLast('/').substringAfterLast('\\')
    val safe = name.replace(Regex("[^A-Za-z0-9._-]"), "_").ifBlank { "game.elf" }
    val dest = File(context.cacheDir, "add-game/$safe")
    dest.parentFile?.mkdirs()
    context.contentResolver.openInputStream(uri).use { input ->
        requireNotNull(input) { "Could not open the selected file." }
        dest.outputStream().use { output -> input.copyTo(output) }
    }
    val sourcePath = uri.path?.let { File(it) }?.takeIf { it.isFile && it.parentFile?.isDirectory == true }
    if (sourcePath != null && sourcePath.parentFile != dest.parentFile) {
        sourcePath.parentFile?.listFiles()?.forEach { sibling ->
            if (sibling.name == "icon.png" || sibling.name == "icon.jpg" || sibling.name == "steam_appid.txt" || sibling.name.endsWith(".desktop")) {
                sibling.copyTo(File(dest.parentFile, sibling.name), overwrite = true)
            }
        }
    }
    dest
}

private fun queryName(context: android.content.Context, uri: Uri): String {
    context.contentResolver.query(uri, arrayOf(android.provider.OpenableColumns.DISPLAY_NAME), null, null, null)?.use { cursor ->
        if (cursor.moveToFirst()) {
            val index = cursor.getColumnIndex(android.provider.OpenableColumns.DISPLAY_NAME)
            if (index >= 0) return cursor.getString(index).orEmpty()
        }
    }
    return uri.lastPathSegment.orEmpty()
}

private fun persist(
    context: android.content.Context,
    staged: File,
    title: String,
    summary: String,
    fileName: String,
    arch: String,
    appId: String,
    envText: String,
    icon: Bitmap?,
): Boolean {
    val safeName = fileName.substringAfterLast('/').replace(Regex("[^A-Za-z0-9._ -]"), "_").trim().ifBlank { return false }
    var id = safeName.substringBeforeLast('.').lowercase().replace(Regex("[^a-z0-9]+"), "-").trim('-').ifBlank { "game" }
    if (GuestCatalog.merged().any { it.id == id }) id = "$id-custom"
    val dir = CustomGuestStore.guestDir(context, id).apply { mkdirs() }
    val executable = File(dir, safeName)
    staged.copyTo(executable, overwrite = true)
    executable.setReadable(true, false)
    executable.setExecutable(true, false)
    var iconFile: String? = null
    if (icon != null) {
        val out = File(dir, "icon.png")
        out.outputStream().use { icon.compress(Bitmap.CompressFormat.PNG, 100, it) }
        iconFile = "icon.png"
    } else {
        listOf("icon.png", "icon.jpg").forEach { name ->
            val sibling = File(staged.parentFile, name)
            if (sibling.isFile) {
                sibling.copyTo(File(dir, name), overwrite = true)
                iconFile = name
            }
        }
    }
    val env = envText.lines().map { it.trim() }.filter { it.contains("=") }.distinct().toMutableList()
    val idValue = appId.trim().takeIf { it.all(Char::isDigit) && it.isNotEmpty() }
    if (idValue != null && env.none { it.startsWith("SteamAppId=") }) env += "SteamAppId=$idValue"
    if (env.none { it.startsWith("DISPLAY=") }) env += "DISPLAY=:0"
    CustomGuestStore.save(
        context,
        GuestProgram(
            id = id,
            title = title.trim(),
            summary = summary.trim(),
            assetPath = "",
            fileName = safeName,
            env = env,
            preinstalled = true,
            arch = arch.ifBlank { "x86_64" },
            steamAppId = idValue,
            iconFile = iconFile,
            userAdded = true,
        ),
    )
    return true
}
