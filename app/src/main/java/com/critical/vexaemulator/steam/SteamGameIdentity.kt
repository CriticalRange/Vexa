package com.critical.vexaemulator.steam

import com.critical.vexaemulator.guest.GuestProgram
import java.util.Locale

/** Store identity is presentation-only: never rewrite the game's launch environment. */
object SteamGameIdentity {
    fun appId(program: GuestProgram): Int? =
        sequenceOf(program.steamAppId, program.env.firstOrNull { it.startsWith("SteamAppId=") }?.substringAfter('='))
            .mapNotNull { it?.trim()?.toIntOrNull()?.takeIf { id -> id > 0 } }.firstOrNull()

    fun normalizedTitle(title: String): String = title.lowercase(Locale.ROOT)
        .replace("™", "").replace("®", "").trim().replace(Regex("\\s+"), " ")

    fun exactMatch(title: String, candidates: List<Pair<Int, String>>): Int? = candidates
        .filter { it.first > 0 && normalizedTitle(it.second) == normalizedTitle(title) }
        .distinctBy { it.first }.singleOrNull()?.first
}
