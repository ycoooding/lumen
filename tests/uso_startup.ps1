param(
    [string] $BuildDirectory = (Join-Path $PSScriptRoot '..\build-lumen')
)
$ErrorActionPreference = 'Stop'
$buildPath = (Resolve-Path -LiteralPath $BuildDirectory).Path
$binary = Join-Path $buildPath 'Lumen.exe'
$expectedVersion = (Get-Content -LiteralPath (Join-Path $PSScriptRoot '..\package.json') -Raw | ConvertFrom-Json).version
$logDirectory = Join-Path $buildPath 'uso-startup-tests'
New-Item -ItemType Directory -Path $logDirectory -Force | Out-Null
Push-Location -LiteralPath $buildPath
try {
    foreach ($case in @('default', 'enabled', 'disabled')) {
        $config = Join-Path $PSScriptRoot "fixtures\uso\$case.conf"
        $log = Join-Path $logDirectory "$case.log"
        $output = (& $binary $config "log_path=$log" --version 2>&1) -join "`n"
        if ($LASTEXITCODE -ne 0) { throw "Startup failed for $case`: $output" }
        if (-not $output.Contains("Lumen version: $expectedVersion ")) {
            throw "Version mismatch for $case`: $output"
        }
        $state = if ($case -eq 'enabled') { 'enabled (udp_segmentation_offload=true)' } else { 'disabled (udp_segmentation_offload=false)' }
        if (-not $output.Contains("UDP segmentation offload (USO): $state")) {
            throw "USO startup log mismatch for $case`: $output"
        }
        if ($output.Contains('bypassing multi-packet batch')) {
            throw 'Fallback should not be logged before any packet is sent.'
        }
        if ((Get-Content -LiteralPath $log -Raw) -notmatch [regex]::Escape("UDP segmentation offload (USO): $state")) {
            throw "USO state missing from log file for $case"
        }
        Write-Output "PASS: $case config, version $expectedVersion, $state"
    }
} finally {
    Pop-Location
}
