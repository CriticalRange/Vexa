param(
  [Parameter(Mandatory=$true)][string]$Out,
  [string]$Tag = "",
  [int]$Seconds = 0
)
# Capture logcat to $Out as UTF-8 (PowerShell's > writes UTF-16 and corrupts it).
$bash = "C:\Program Files\Git\bin\bash.exe"
if ($Seconds -gt 0) { Start-Sleep -Seconds $Seconds }
$cmd = "adb logcat -d -v threadtime > /c/Users/zeytu/Documents/vexa/.il2cpp/_tmp.log 2>&1"
& $bash -c $cmd
Copy-Item "C:\Users\zeytu\Documents\vexa\.il2cpp\_tmp.log" $Out -Force
Remove-Item "C:\Users\zeytu\Documents\vexa\.il2cpp\_tmp.log" -ErrorAction SilentlyContinue
$n = (Get-Content $Out | Measure-Object -Line).Lines
Write-Output "captured $n lines -> $Out"