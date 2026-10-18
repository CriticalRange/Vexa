package com.critical.vexaemulator.steam

import com.critical.vexaemulator.steam.library.SteamDepotDownloader
import `in`.dragonbra.javasteam.types.KeyValue
import org.junit.Assert.*
import org.junit.Test

class SteamDepotSelectionTest {
    private fun node(name: String, vararg children: KeyValue) = KeyValue(name).apply { this.children.addAll(children) }
    private fun depot(id: Int, os: String, arch: String = "64", dlc: Boolean = false) = node(id.toString(),
        node("config", KeyValue("oslist", os), KeyValue("osarch", arch)),
        node("manifests", node("public", KeyValue("gid", "123"))),
        KeyValue("dlcappid", if (dlc) "999" else "0"),
    )

    @Test fun linuxNeedsSharedFilesButNotWindowsOrUnownedDlc() {
        val app = node("app", node("common", KeyValue("oslist", "windows,linux")),
            node("depots", depot(1, "linux"), depot(2, ""), depot(3, "windows"), depot(4, "linux", dlc = true), depot(5, "linux", "32")))
        assertEquals(listOf(1, 2, 5), SteamDepotDownloader.linuxDepots(app, 100).map { it.depotId })
    }

    @Test fun neutralDepotDoesNotMakeWindowsOnlyGameInstallable() {
        val app = node("app", node("common", KeyValue("oslist", "windows")), node("depots", depot(1, "")))
        assertTrue(SteamDepotDownloader.linuxDepots(app, 100).isEmpty())
    }

    @Test fun missingPublicManifestIsNotDownloadable() {
        val app = node("app", node("common", KeyValue("oslist", "linux")), node("depots", node("1", node("config", KeyValue("oslist", "linux")))))
        assertTrue(SteamDepotDownloader.linuxDepots(app, 100).isEmpty())
    }
}
