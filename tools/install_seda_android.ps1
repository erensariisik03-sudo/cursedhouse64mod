[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^[A-Za-z0-9_]+(\.[A-Za-z0-9_]+)+$')]
    [string]$PackageName,
    [string]$ModelArchive = (Join-Path $PSScriptRoot '..\dist\seda-v0.2-tiny-c32.zip'),
    [string]$Adb = 'adb'
)
$ErrorActionPreference = 'Stop'
$AdbCommand = Get-Command $Adb -ErrorAction Stop
if (-not (Test-Path -LiteralPath $ModelArchive -PathType Leaf)) {
    throw "SEDA model archive not found: $ModelArchive. Download the GitHub Actions artifact first."
}
$Temp = Join-Path ([IO.Path]::GetTempPath()) ("gmm-seda-" + [Guid]::NewGuid().ToString('N'))
try {
    New-Item -ItemType Directory -Path $Temp | Out-Null
    Expand-Archive -LiteralPath $ModelArchive -DestinationPath $Temp -Force
    $ModelRoot = Join-Path $Temp 'gmm_seda'
    foreach ($File in @('tokens.txt', 'c32/encoder.int8.onnx', 'c32/decoder.onnx', 'c32/joiner.int8.onnx')) {
        if (-not (Test-Path -LiteralPath (Join-Path $ModelRoot $File) -PathType Leaf)) {
            throw "Model archive is missing required file: $File"
        }
    }
    $Remote = "/sdcard/Android/data/$PackageName/files/gmm_seda"
    & $AdbCommand.Source shell "mkdir -p '$Remote/c32'"
    if ($LASTEXITCODE -ne 0) { throw 'adb could not create the app-specific model folder.' }
    & $AdbCommand.Source push (Join-Path $ModelRoot 'tokens.txt') "$Remote/tokens.txt"
    if ($LASTEXITCODE -ne 0) { throw 'Could not push tokens.txt. Android scoped-storage restrictions may block this path.' }
    foreach ($File in @('encoder.int8.onnx', 'decoder.onnx', 'joiner.int8.onnx')) {
        & $AdbCommand.Source push (Join-Path $ModelRoot "c32/$File") "$Remote/c32/$File"
        if ($LASTEXITCODE -ne 0) { throw "Could not push c32/$File" }
    }
    Write-Host "SEDA v0.2-Tiny installed at $Remote" -ForegroundColor Green
    Write-Host 'Restart the game process before enabling Voice AI.'
}
finally {
    if (Test-Path -LiteralPath $Temp) { Remove-Item -LiteralPath $Temp -Recurse -Force }
}
