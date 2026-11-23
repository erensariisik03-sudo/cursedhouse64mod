# Windows PowerShell 5.1 / PowerShell 7. Run before starting the game.
[CmdletBinding()]
param(
    [ValidatePattern('^[A-Za-z][A-Za-z0-9_]*(\.[A-Za-z0-9_]+)+$')]
    [string]$PackageName = 'com.UnityUserco.EscapeHauntedHouseOnline',
    [ValidatePattern('^[A-Za-z0-9_.:-]+$')]
    [string]$Serial,
    [ValidateRange(0,86400)]
    [int]$Seconds = 0,
    [string]$OutputDirectory,
    [string]$AdbPath
)
$ErrorActionPreference = 'Stop'
if (-not $AdbPath) {
    if (Test-Path -LiteralPath (Join-Path $PSScriptRoot 'adb.exe') -PathType Leaf) {
        $AdbPath = Join-Path $PSScriptRoot 'adb.exe'
    } elseif (Test-Path -LiteralPath '.\adb.exe' -PathType Leaf) {
        $AdbPath = (Resolve-Path -LiteralPath '.\adb.exe').Path
    } else {
        $adbCommand = Get-Command adb -CommandType Application -ErrorAction SilentlyContinue
        if (-not $adbCommand) { throw 'ADB bulunamadi. Betigi platform-tools klasorunde calistir veya -AdbPath belirt.' }
        $AdbPath = $adbCommand.Source
    }
}
$AdbPath = (Resolve-Path -LiteralPath $AdbPath).Path
$deviceLines = @(& $AdbPath devices)
if ($LASTEXITCODE -ne 0) { throw 'ADB cihaz listesi okunamadi.' }
$devices = @($deviceLines | ForEach-Object {
    if ($_ -match '^([^\s]+)\s+device\s*$') { $Matches[1] }
})
if ($Serial) {
    if ($devices -notcontains $Serial) { throw 'Secilen cihaz hazir degil. USB hata ayiklama iznini onayla.' }
} elseif ($devices.Count -eq 1) {
    $Serial = $devices[0]
} elseif ($devices.Count -eq 0) {
    throw 'Hazir cihaz yok. USB baglantisini ve telefondaki hata ayiklama iznini kontrol et.'
} else {
    throw 'Birden fazla cihaz var. -Serial ile cihaz sec (adb devices).'
}
# Start-Process joins ArgumentList on Windows; keep device identifiers single tokens.
if ($Serial -notmatch '^[A-Za-z0-9_.:-]+$') { throw 'Cihaz seri numarasi desteklenmeyen karakter iceriyor.' }
if (-not $OutputDirectory) {
    $OutputDirectory = [Environment]::GetFolderPath('MyDocuments')
    if (-not $OutputDirectory) { $OutputDirectory = (Get-Location).Path }
}
$captureName = 'gmm-gas-god-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff')
$captureDirectory = Join-Path $OutputDirectory $captureName
New-Item -ItemType Directory -Path $captureDirectory -Force | Out-Null
$adbArguments = @('-s', $Serial)
$logger = $null
function Save-AdbText {
    param([string]$Name, [string[]]$Arguments)
    $ErrorActionPreference = 'Continue'
    try {
        $lines = @(& $AdbPath @adbArguments @Arguments 2>&1)
        $code = $LASTEXITCODE
        @($lines; "ADB exit code: $code") | Out-File -LiteralPath (Join-Path $captureDirectory $Name) -Encoding utf8
    } catch {
        $_ | Out-File -LiteralPath (Join-Path $captureDirectory $Name) -Encoding utf8
    }
}
function Save-Snapshot {
    param([string]$Label)
    Get-Date -Format o | Out-File -LiteralPath (Join-Path $captureDirectory ($Label + '-time.txt')) -Encoding utf8
    Save-AdbText ($Label + '-exit-info.txt') @('shell','dumpsys','activity','exit-info',$PackageName)
    Save-AdbText ($Label + '-memory.txt') @('shell','dumpsys','meminfo',$PackageName)
    Save-AdbText ($Label + '-system-memory.txt') @('shell','cat','/proc/meminfo')
    Save-AdbText ($Label + '-memory-pressure.txt') @('shell','cat','/proc/pressure/memory')
    Save-AdbText ($Label + '-pid.txt') @('shell','pidof',$PackageName)
}
@("Package: $PackageName", "Device: $Serial", "Start: $(Get-Date -Format o)", 'Build marker: voice-ai-panel-latebind-arm64') |
    Out-File -LiteralPath (Join-Path $captureDirectory 'session.txt') -Encoding utf8
Save-AdbText 'android-version.txt' @('shell','getprop','ro.build.version.release')
Save-AdbText 'android-sdk.txt' @('shell','getprop','ro.build.version.sdk')
Save-AdbText 'cpu-abi.txt' @('shell','getprop','ro.product.cpu.abilist')
Save-AdbText 'game-package.txt' @('shell','dumpsys','package',$PackageName)
Save-Snapshot 'before'
# Keep pre-existing crash evidence; never clear logcat, stop the game or restart ADB.
Save-AdbText 'before-logcat.txt' @('logcat','-b','all','-d','-t','300','-v','threadtime')
try {
    $logger = Start-Process -FilePath $AdbPath -ArgumentList (@($adbArguments) + @('logcat','-b','all','-v','threadtime','-T','1')) `
        -RedirectStandardOutput (Join-Path $captureDirectory 'logcat.txt') `
        -RedirectStandardError (Join-Path $captureDirectory 'logcat-errors.txt') -NoNewWindow -PassThru
    Start-Sleep -Milliseconds 300
    $logger.Refresh()
    if ($logger.HasExited) { throw 'Logcat baslatilamadi. logcat-errors.txt dosyasini kontrol et.' }
    Write-Host "Kayit basladi: $captureDirectory"
    Write-Host 'Oyunu ac; God Mode, benzin patlamasi ve Crossbow/Shotgun atisini dene.'
    Write-Host 'Once asillari kaldirip uc tur klonla; God kapaliyken gorus ve esya atma sesini dene.'
    Write-Host 'AI > Sesli AI: ses-ses, metin-ses, ses-metin; acik/kapali sesli odada dene.'
    Write-Host 'AI konusurken mikrofon ve durdurma/oda degisimi sonrasi eski mikrofon durumunu kontrol et.'
    Write-Host 'Host ve client icin /evet-/hayir saka oylamasini ve Durdur/Devam et dugmelerini dene.'
    if ($Seconds -gt 0) {
        $deadline = (Get-Date).AddSeconds($Seconds)
        while ((Get-Date) -lt $deadline) {
            Start-Sleep -Milliseconds 250
            $logger.Refresh()
            if ($logger.HasExited) { Write-Warning 'ADB kaydi durdu; mevcut kayitlar paketleniyor.'; break }
        }
    } else {
        Read-Host 'Deneme tamamlaninca buraya donup ENTER tusuna bas' | Out-Null
    }
} finally {
    if ($logger) {
        $logger.Refresh()
        if (-not $logger.HasExited) {
            Stop-Process -Id $logger.Id -ErrorAction SilentlyContinue
        }
        $logger.WaitForExit()
        $logger.Dispose()
    }
    Save-Snapshot 'after'
    Save-AdbText 'crash-buffer.txt' @('logcat','-b','crash','-d','-v','threadtime')
    Save-AdbText 'after-activity.txt' @('shell','dumpsys','activity','activities')
    $logPath = Join-Path $captureDirectory 'logcat.txt'
    if (Test-Path -LiteralPath $logPath) {
        Select-String -LiteralPath $logPath -Pattern 'GMM_DIAG|multiplayermod|FATAL EXCEPTION|Fatal signal|SIGSEGV|SIGABRT|ANR in|lmkd|lowmemorykiller' |
            ForEach-Object { $_.Line } | Out-File -LiteralPath (Join-Path $captureDirectory 'diagnostics.txt') -Encoding utf8
    }
    $archivePath = $captureDirectory + '.zip'
    try {
        Compress-Archive -Path (Join-Path $captureDirectory '*') -DestinationPath $archivePath -Force
        Write-Host "Gonderecegin kayit: $archivePath"
    } catch {
        Write-Warning "ZIP olusmadi. Kayitlar bu klasorde: $captureDirectory"
    }
}
