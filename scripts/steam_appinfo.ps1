# Authoritative Linux/bitness check for Steam apps, using SteamCMD's app_info_print.
#
# The store API (store.steampowered.com/api/appdetails) reports platforms.linux
# as false for games that clearly ship native Linux builds, so it cannot be
# trusted here. SteamCMD reads the real appinfo VDF from Steam's content
# servers, which carries oslist and osarch.
#
# Usage:
#   powershell -File steam_appinfo.ps1 -AppIds 632360,255710,304930
#   powershell -File steam_appinfo.ps1 -FromCsv C:\path\steam_library.csv

param(
  # Comma separated rather than [int[]]: powershell.exe -File does not bind
  # array parameters from the command line.
  [string]$AppIds = "",
  [string]$FromCsv = "",
  [string]$SteamCmd = "C:\steamcmd\steamcmd.exe"
)

$ErrorActionPreference = "Continue"

$ids = @()
foreach ($part in ($AppIds -split "[,\s]+")) {
  if ($part -match "^\d+$") { $ids += [int]$part }
}
if ($FromCsv) {
  $ids += (Import-Csv $FromCsv | ForEach-Object { [int]$_.AppID })
}
$AppIdList = $ids | Sort-Object -Unique
if ($AppIdList.Count -eq 0) {
  Write-Output "AppIds verilmedi."
  exit 1
}
if (-not (Test-Path $SteamCmd)) {
  Write-Output "steamcmd bulunamadi: $SteamCmd"
  exit 1
}

# One steamcmd run with many +app_info_print commands is far faster than one
# process per app, and it reuses a single authenticated session.
$argList = @("+quit")
foreach ($id in $AppIdList) { $argList = @("+app_info_print $id") + $argList }

$raw = & $SteamCmd @argList 2>&1 | Out-String

# app_info_print prints "AppID : <id>, change number : ..." then a VDF block whose
# keys we want live under "common". Names repeat in depots, so only the first
# value seen after an AppID line is taken.
$results = @()
foreach ($line in ($raw -split "`r?`n")) {
  if ($line -match 'AppID\s*:\s*(\d+)') {
    $results += [pscustomobject]@{
      AppID = [int]$matches[1]; Name = ""; OsList = ""; OsArch = ""; Type = ""
    }
    continue
  }
  if ($results.Count -eq 0) { continue }
  $last = $results[-1]
  if ($line -match '"oslist"\s+"([^"]*)"') { if (-not $last.OsList) { $last.OsList = $matches[1] } }
  elseif ($line -match '"osarch"\s+"([^"]*)"') { if (-not $last.OsArch) { $last.OsArch = $matches[1] } }
  elseif ($line -match '"type"\s+"([^"]*)"') { if (-not $last.Type) { $last.Type = $matches[1] } }
  elseif ($line -match '"name"\s+"([^"]*)"') { if (-not $last.Name) { $last.Name = $matches[1] } }
}

Write-Output "=== Steam appinfo (oslist / osarch) ==="
Write-Output "oslist: linux = Linux destegi,  osarch: 64 = 64-bit"
$results | Where-Object { $_.OsList -match "linux" -and $_.Type -eq "Game" } |
  Sort-Object Name | Format-Table -AutoSize AppID, OsList, OsArch, Name

$all = $results | Where-Object { $_.Type -eq "Game" }
Write-Output ""
Write-Output "game toplam        : $($all.Count)"
Write-Output "linux olan         : $(($all | Where-Object { $_.OsList -match 'linux' }).Count)"
Write-Output "linux + 64-bit     : $(($all | Where-Object { $_.OsList -match 'linux' -and $_.OsArch -match '64' }).Count)"

$results | Export-Csv -NoTypeInformation -Encoding UTF8 (Join-Path $PSScriptRoot "steam_appinfo.csv")
Write-Output "csv: $(Join-Path $PSScriptRoot 'steam_appinfo.csv')"
