package com.critical.vexaemulator.steam

import com.critical.vexaemulator.guest.GuestProgram
import org.junit.Assert.*
import org.junit.Test

class SteamGameIdentityTest {
    private fun game(id: String? = null, env: List<String> = emptyList()) =
        GuestProgram("test", "Celeste", "", "", "game", steamAppId = id, env = env)

    @Test fun explicitIdWinsOverEnvironment() {
        assertEquals(504230, SteamGameIdentity.appId(game("504230", listOf("SteamAppId=588650"))))
    }

    @Test fun environmentSupportsExistingCatalog() {
        assertEquals(588650, SteamGameIdentity.appId(game(env = listOf("SteamAppId=588650"))))
        assertEquals(588650, SteamGameIdentity.appId(game("invalid", listOf("SteamAppId=588650"))))
        assertNull(SteamGameIdentity.appId(game("0", listOf("SteamAppId=-1"))))
    }

    @Test fun neverSelectFirstFuzzyResultOrSoundtrack() {
        assertEquals(504230, SteamGameIdentity.exactMatch("Celeste", listOf(1092840 to "Celeste Soundtrack", 504230 to "Celeste")))
        assertNull(SteamGameIdentity.exactMatch("Celeste", listOf(1092840 to "Celeste Soundtrack")))
        assertNull(SteamGameIdentity.exactMatch("Game", listOf(1 to "Game", 2 to "Game")))
        assertNull(SteamGameIdentity.exactMatch("Game II", listOf(1 to "Game")))
    }

    @Test fun onlyCosmeticTitleDifferencesAreIgnored() {
        assertEquals(1, SteamGameIdentity.exactMatch("  DEAD   CELLS™ ", listOf(1 to "Dead Cells")))
    }
}
