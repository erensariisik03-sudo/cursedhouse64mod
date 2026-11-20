$ErrorActionPreference = 'Stop'
if (Get-Command adb -ErrorAction SilentlyContinue) {
    $adbPath = (Get-Command adb).Source
} elseif (Test-Path '.\adb.exe') {
    $adbPath = (Resolve-Path '.\adb.exe').Path
} else {
    throw 'ADB bulunamadi. platform-tools klasorunde PowerShell acip bu betigi calistir.'
}
$gamePackage = 'com.UnityUserco.EscapeHauntedHouseOnline'
$documentsPath = [Environment]::GetFolderPath('MyDocuments')
$captureDirectory = Join-Path $documentsPath ('gmm-v19-emotes-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $captureDirectory -Force | Out-Null
& $adbPath get-state
if ($LASTEXITCODE -ne 0) { throw 'ADB baglantisini ve telefondaki USB hata ayiklama iznini kontrol et.' }

function Save-Snapshot([string]$label) {
    # Unsupported commands/permission errors are kept in the report as evidence.
    $previousPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        Get-Date -Format o | Out-File (Join-Path $captureDirectory ($label + '-time.txt')) -Encoding utf8
        & $adbPath shell dumpsys activity exit-info $gamePackage 2>&1 |
            Out-File (Join-Path $captureDirectory ($label + '-exit-info.txt')) -Encoding utf8
        & $adbPath shell dumpsys meminfo $gamePackage 2>&1 |
            Out-File (Join-Path $captureDirectory ($label + '-game-meminfo.txt')) -Encoding utf8
        & $adbPath shell cat /proc/meminfo 2>&1 |
            Out-File (Join-Path $captureDirectory ($label + '-system-memory.txt')) -Encoding utf8
        & $adbPath shell cat /proc/pressure/memory 2>&1 |
            Out-File (Join-Path $captureDirectory ($label + '-memory-pressure.txt')) -Encoding utf8
    } finally { $ErrorActionPreference = $previousPreference }
}

Save-Snapshot 'before'
& $adbPath logcat -b all -c
if ($LASTEXITCODE -ne 0) { throw 'Log tamponu temizlenemedi.' }
$adbLogProcess = $null
try {
    $adbLogProcess = Start-Process -FilePath $adbPath -ArgumentList @('logcat','-b','all','-v','threadtime') `
        -RedirectStandardOutput (Join-Path $captureDirectory 'logcat.txt') `
        -RedirectStandardError (Join-Path $captureDirectory 'logcat-errors.txt') -NoNewWindow -PassThru
    Write-Host "Kayit klasoru: $captureDirectory"
    Write-Host 'Oyunu ac, odaya gir ve beklemede dans dugmesini dene. Sonra buraya don.'
    Read-Host 'Dugme/dans denemesi bittikten sonra ENTER tusuna bas' | Out-Null
} finally {
    if ($adbLogProcess -and -not $adbLogProcess.HasExited) {
        # Stop only the logger created by this script, never the game/ADB server.
        Stop-Process -Id $adbLogProcess.Id -ErrorAction SilentlyContinue
        $adbLogProcess.WaitForExit()
    }
    Save-Snapshot 'after'
    $ErrorActionPreference = 'Continue'
    & $adbPath logcat -b crash -d -v threadtime 2>&1 |
        Out-File (Join-Path $captureDirectory 'crash-buffer.txt') -Encoding utf8
    & $adbPath shell dumpsys activity processes 2>&1 |
        Out-File (Join-Path $captureDirectory 'after-processes.txt') -Encoding utf8
    $archivePath = $captureDirectory + '.zip'
    Compress-Archive -Path (Join-Path $captureDirectory '*') -DestinationPath $archivePath -Force
    if (Test-Path $archivePath) { Write-Host "Gonderecegin dosya: $archivePath" }
    else { Write-Host "Zip olusmadi; kayit klasorunu gonder: $captureDirectory" }
}
