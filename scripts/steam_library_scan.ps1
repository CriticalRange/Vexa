# Steam library scan: read the local Steam client account file, then ask the
# Steam store API which of those apps ship a native Linux build.
#
# No API key needed: store.steampowered.com/api/appdetails is public.
# The localconfig.vdf list only covers apps the client has touched, so this is a
# subset of the owned library. For the full list use steam_web_api.txt as a key.

param(
  [string]$SteamPath = "C:\Program Files (x86)\Steam"
)

$ErrorActionPreference = "Continue"

$userdata = Join-Path $SteamPath "userdata"
$account = Get-ChildItem $userdata -Directory | Select-Object -First 1
if (-not $account) {
  Write-Output "userdata altinda hesap yok: $userdata"
  exit 1
}
$accountId = $account.Name
# SteamID64 = 76561197960265728 + account id
$steamId64 = [uint64]76561197960265728 + [uint64]$accountId

$cfg = Join-Path $account.FullName "config\localconfig.vdf"
$text = Get-Content $cfg -Raw
$appIds = [regex]::Matches($text, '"(\d{4,6})"') |
  ForEach-Object { $_.Groups[1].Value } |
  Where-Object { $_ -ne "0" } |
  Sort-Object -Unique

Write-Output "hesap (account id) : $accountId"
Write-Output "SteamID64          : $steamId64"
Write-Output "aday aday sayisi   : $($appIds.Count)"
Write-Output ""

$rows = @()
foreach ($id in $appIds) {
  # filters=basic drops the "platforms" key entirely, which silently made every
  # row look Windows-only. Ask for the full payload.
  $url = "https://store.steampowered.com/api/appdetails?appids=$id&cc=us&l=english"
  try {
    $r = Invoke-RestMethod -Uri $url -TimeoutSec 20 -Headers @{ "User-Agent" = "Mozilla/5.0" }
  } catch {
    Start-Sleep -Milliseconds 1200
    continue
  }
  $node = $r.$id
  if (-not $node.success) { continue }
  $d = $node.data
  $rows += [pscustomobject]@{
    AppID   = [int]$id
    Name    = $d.name
    Linux   = [bool]$d.platforms.linux
    Windows = [bool]$d.platforms.windows
    Type    = $d.type
  }
  Start-Sleep -Milliseconds 350
}

Write-Output "=== LINUX DESTEKLEYENLER ==="
$rows | Where-Object { $_.Linux -and $_.Type -eq "game" } | Sort-Object Name |
  Format-Table -AutoSize AppID, Name

Write-Output ""
Write-Output "=== hepsi (linux isareti) ==="
$rows | Sort-Object @{e = { -not $_.Linux }}, Name |
  Format-Table -AutoSize AppID, Linux, Name

$rows | Export-Csv -NoTypeInformation -Encoding UTF8 (Join-Path $PSScriptRoot "steam_library.csv")
Write-Output ""
Write-Output "csv yazildi: $(Join-Path $PSScriptRoot 'steam_library.csv')"
