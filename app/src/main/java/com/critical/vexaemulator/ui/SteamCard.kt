package com.critical.vexaemulator.ui

import android.net.Uri
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.OutlinedTextFieldDefaults
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.text.input.VisualTransformation
import androidx.compose.ui.unit.dp
import com.critical.vexaemulator.steam.GuardKind
import com.critical.vexaemulator.steam.SteamLibraries
import com.critical.vexaemulator.steam.SteamLink
import com.critical.vexaemulator.steam.SteamUiState
import com.critical.vexaemulator.steam.SteamworksHost
import com.critical.vexaemulator.ui.theme.Typography
import com.critical.vexaemulator.ui.theme.VexaTheme
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

@Composable
fun SteamCard(modifier: Modifier = Modifier) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    val steam by SteamLink.state.collectAsState()
    val libraries by SteamworksHost.libraries.collectAsState()
    LaunchedEffect(Unit) {
        SteamLink.restore(context)
    }

    val importApi = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        if (uri != null) scope.launch { importLibrary(context, uri, api = true) }
    }
    val importClient = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        if (uri != null) scope.launch { importLibrary(context, uri, api = false) }
    }

    Column(modifier) {
        SectionLabel(text = "Steam")
        Spacer(modifier = Modifier.height(10.dp))
        SectionCard(modifier = Modifier.fillMaxWidth()) {
            Column(modifier = Modifier.padding(16.dp)) {
                AccountBlock(state = steam)
                Spacer(modifier = Modifier.height(14.dp))
                LibraryBlock(
                    libraries = libraries,
                    onImportApi = { importApi.launch(arrayOf("*/*")) },
                    onImportClient = { importClient.launch(arrayOf("*/*")) },
                )
            }
        }
    }
}

@Composable
private fun AccountBlock(state: SteamUiState) {
    val context = LocalContext.current
    var account by remember { mutableStateOf("") }
    var password by remember { mutableStateOf("") }
    var guard by remember { mutableStateOf("") }
    val guardAttempt = (state as? SteamUiState.NeedGuard)?.attempt
    LaunchedEffect(guardAttempt) {
        if (guardAttempt != null) guard = ""
    }
    val c = VexaTheme.colors

    when (state) {
        is SteamUiState.SignedIn -> {
            Text(text = state.accountName, style = Typography.titleMedium, color = c.textPrimary)
            Spacer(modifier = Modifier.height(4.dp))
            Text(
                text = state.steamId,
                style = Typography.bodyMedium.copy(fontFamily = FontFamily.Monospace, color = c.textSecondary),
            )
            Spacer(modifier = Modifier.height(8.dp))
            TinyPill(
                text = if (state.online) "online" else "saved session",
                color = if (state.online) c.success else c.warning,
            )
            Spacer(modifier = Modifier.height(12.dp))
            GhostButton(
                text = "Sign out",
                onClick = { SteamLink.signOut(context) },
                modifier = Modifier.fillMaxWidth(),
            )
        }

        is SteamUiState.NeedGuard -> {
            Text(text = state.hint, style = Typography.bodyMedium, color = c.textPrimary)
            if (state.retry) {
                Spacer(modifier = Modifier.height(6.dp))
                Text(text = "That code was not accepted", style = Typography.bodyMedium, color = c.error)
            }
            Spacer(modifier = Modifier.height(10.dp))
            SteamField(
                value = guard,
                onValueChange = { guard = it },
                label = if (state.kind == GuardKind.Confirm) "Steam Guard code" else "Code",
            )
            Spacer(modifier = Modifier.height(10.dp))
            VexaButton(
                text = "Submit code",
                onClick = { SteamLink.submitGuard(guard) },
                icon = null,
                enabled = guard.isNotBlank(),
                modifier = Modifier.fillMaxWidth(),
            )
            Spacer(modifier = Modifier.height(8.dp))
            GhostButton(
                text = "Cancel",
                onClick = { SteamLink.cancel() },
                modifier = Modifier.fillMaxWidth(),
            )
        }

        else -> {
            val message = when (state) {
                is SteamUiState.Busy -> state.message
                is SteamUiState.Failed -> state.message
                else -> "Sign in with the account that owns the game"
            }
            val messageColor = if (state is SteamUiState.Failed) c.error else c.textSecondary
            Text(text = message, style = Typography.bodyMedium, color = messageColor)
            Spacer(modifier = Modifier.height(10.dp))
            SteamField(value = account, onValueChange = { account = it }, label = "Account")
            Spacer(modifier = Modifier.height(8.dp))
            SteamField(
                value = password,
                onValueChange = { password = it },
                label = "Password",
                password = true,
            )
            Spacer(modifier = Modifier.height(10.dp))
            val busy = state is SteamUiState.Busy
            VexaButton(
                text = if (busy) "Signing in" else "Sign in",
                onClick = {
                    val typed = password
                    password = ""
                    SteamLink.signIn(context, account, typed)
                },
                icon = null,
                enabled = !busy && account.isNotBlank() && password.isNotBlank(),
                modifier = Modifier.fillMaxWidth(),
            )
        }
    }
}

@Composable
private fun LibraryBlock(
    libraries: SteamLibraries,
    onImportApi: () -> Unit,
    onImportClient: () -> Unit,
) {
    val c = VexaTheme.colors
    Text(
        text = "androidarm64",
        style = Typography.labelMedium.copy(fontFamily = FontFamily.Monospace, color = c.textSecondary),
    )
    Spacer(modifier = Modifier.height(6.dp))
    Text(
        text = "libsteam_api.so ${if (libraries.apiPresent) "present" else "missing"}",
        style = Typography.bodyMedium.copy(fontFamily = FontFamily.Monospace),
        color = if (libraries.apiPresent) c.success else c.textSecondary,
    )
    Text(
        text = "libsteamclient.so ${if (libraries.clientPresent) "present" else "missing"}",
        style = Typography.bodyMedium.copy(fontFamily = FontFamily.Monospace),
        color = if (libraries.clientPresent) c.success else c.textSecondary,
    )
    Spacer(modifier = Modifier.height(4.dp))
    Text(
        text = libraries.status,
        style = Typography.bodyMedium,
        color = c.plasma,
    )
    Spacer(modifier = Modifier.height(10.dp))
    GhostButton(text = "Import libsteam_api.so", onClick = onImportApi, modifier = Modifier.fillMaxWidth())
    Spacer(modifier = Modifier.height(8.dp))
    GhostButton(text = "Import libsteamclient.so", onClick = onImportClient, modifier = Modifier.fillMaxWidth())
}

@Composable
private fun SteamField(
    value: String,
    onValueChange: (String) -> Unit,
    label: String,
    password: Boolean = false,
) {
    val c = VexaTheme.colors
    OutlinedTextField(
        value = value,
        onValueChange = onValueChange,
        label = { Text(label) },
        singleLine = true,
        visualTransformation = if (password) PasswordVisualTransformation() else VisualTransformation.None,
        keyboardOptions = KeyboardOptions(
            keyboardType = if (password) KeyboardType.Password else KeyboardType.Text,
        ),
        modifier = Modifier.fillMaxWidth(),
        colors = OutlinedTextFieldDefaults.colors(
            focusedTextColor = c.textPrimary,
            unfocusedTextColor = c.textPrimary,
            focusedBorderColor = c.plasma,
            unfocusedBorderColor = c.hairline,
            focusedLabelColor = c.plasma,
            unfocusedLabelColor = c.textSecondary,
            unfocusedPlaceholderColor = c.textSecondary,
            cursorColor = c.plasma,
        ),
    )
}

private suspend fun importLibrary(context: android.content.Context, uri: Uri, api: Boolean) {
    val message = withContext(Dispatchers.IO) {
        if (api) SteamworksHost.importApi(context, uri) else SteamworksHost.importClient(context, uri)
    }
    if (message.startsWith("Saved")) {
        withContext(Dispatchers.IO) { SteamworksHost.prepare(context, "") }
    }
}
