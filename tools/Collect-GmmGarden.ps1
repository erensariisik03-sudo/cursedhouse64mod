$ErrorActionPreference = 'Stop'
if (Get-Command adb -ErrorAction SilentlyContinue) {
    $adbPath = (Get-Command adb).Source
} elseif (Test-Path '.\adb.exe') {
    $adbPath = (Resolve-Path '.\adb.exe').Path
} else {
    throw 'ADB bulunamadi. platform-tools klasorunde PowerShell acip tekrar calistir.'
}
$gamePackage = 'com.UnityUserco.EscapeHauntedHouseOnline'
$documentsPath = [Environment]::GetFolderPath('MyDocuments')
$captureDirectory = Join-Path $documentsPath ('gmm-bahce-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $captureDirectory -Force | Out-Null
& $adbPath get-state
if ($LASTEXITCODE -ne 0) { throw 'ADB iznini kontrol et. Yalnizca test telefonunu USB/ADB ile bagli tut.' }

function Save-State([string]$label) {
    $previousPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        Get-Date -Format o | Out-File (Join-Path $captureDirectory ($label + '-time.txt')) -Encoding utf8
        & $adbPath shell pidof $gamePackage 2>&1 |
            Out-File (Join-Path $captureDirectory ($label + '-pid.txt')) -Encoding utf8
        & $adbPath shell dumpsys activity exit-info $gamePackage 2>&1 |
            Out-File (Join-Path $captureDirectory ($label + '-exit-info.txt')) -Encoding utf8
    } finally { $ErrorActionPreference = $previousPreference }
}

Save-State 'before'
# Restart only the game so the native mod's startup/version markers are captured.
& $adbPath shell am force-stop $gamePackage
if ($LASTEXITCODE -ne 0) { throw 'Oyun kapatilamadi.' }
& $adbPath logcat -b all -c
if ($LASTEXITCODE -ne 0) { throw 'Log tamponu temizlenemedi.' }
$adbLogProcess = $null
try {
    $adbLogProcess = Start-Process -FilePath $adbPath -ArgumentList @('logcat','-b','all','-v','threadtime') `
        -RedirectStandardOutput (Join-Path $captureDirectory 'logcat.txt') `
        -RedirectStandardError (Join-Path $captureDirectory 'logcat-errors.txt') -NoNewWindow -PassThru
    Write-Host 'KAYIT BASLADI. Simdi oyunu telefondan elle ac.'
    Write-Host '1) Magazayi kontrol et, sonra bir odaya gir.'
    Write-Host '2) Bahcedeki bekleme alaninda en az 20 saniye bekle; dans dugmesini kontrol et.'
    Write-Host '3) Klavyeyi acip birkac harf yaz; klavyeyi kapat. Varsa dans dugmesini dene.'
    Write-Host '4) PowerShell penceresine don. Atarsa oyunu tekrar acmadan buraya don.'
    Read-Host 'Deneme bittiginde ENTER tusuna bas (Ctrl+C kullanma)' | Out-Null
} finally {
    if ($adbLogProcess -and -not $adbLogProcess.HasExited) {
        Stop-Process -Id $adbLogProcess.Id -ErrorAction SilentlyContinue
        $adbLogProcess.WaitForExit()
    }
    Save-State 'after'
    $ErrorActionPreference = 'Continue'
    & $adbPath shell getprop ro.product.cpu.abilist 2>&1 |
        Out-File (Join-Path $captureDirectory 'device-abi.txt') -Encoding utf8
    & $adbPath shell dumpsys package $gamePackage 2>&1 |
        Select-String -Pattern 'versionName=|versionCode=|primaryCpuAbi=|secondaryCpuAbi=|nativeLibraryDir=|legacyNativeLibraryDir=' |
        Out-File (Join-Path $captureDirectory 'game-version-abi.txt') -Encoding utf8
    & $adbPath logcat -b crash -d -v threadtime 2>&1 |
        Out-File (Join-Path $captureDirectory 'crash-buffer.txt') -Encoding utf8
    $logPath = Join-Path $captureDirectory 'logcat.txt'
    if (Test-Path $logPath) {
        Select-String -Path $logPath -Encoding UTF8 -Pattern 'multiplayermod|lobby-emotes|FATAL EXCEPTION|Fatal signal' |
            ForEach-Object { $_.Line } |
            Out-File (Join-Path $captureDirectory 'mod-summary.txt') -Encoding utf8
    }
    $archivePath = $captureDirectory + '.zip'
    Compress-Archive -Path (Join-Path $captureDirectory '*') -DestinationPath $archivePath -Force
    if (Test-Path $archivePath) { Write-Host "BANA GONDERECEGIN DOSYA: $archivePath" }
    else { Write-Host "Zip olusmadi. Kayit klasoru: $captureDirectory" }
}
