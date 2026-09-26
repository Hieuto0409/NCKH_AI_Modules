param(
    [string[]]$Environment = "esp32-s3-devkitc-1"
)

$ErrorActionPreference = "Stop"
$projectPath = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
$buildPath = Join-Path $tempRoot ("ppgfw-build-" + $PID)
$buildPathFull = [IO.Path]::GetFullPath($buildPath)
if (-not $buildPathFull.StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase) -or
    -not ([IO.Path]::GetFileName($buildPathFull)).StartsWith("ppgfw-build-")) {
    throw "Unsafe temporary build path: $buildPathFull"
}

$pioCommand = Get-Command pio -ErrorAction SilentlyContinue
if ($null -ne $pioCommand) {
    $pioPath = $pioCommand.Source
}
else {
    $pioPath = Join-Path $env:USERPROFILE ".platformio\penv\Scripts\platformio.exe"
}
if (-not (Test-Path -LiteralPath $pioPath)) {
    throw "PlatformIO executable not found: $pioPath"
}

try {
    New-Item -ItemType Directory -Path $buildPathFull -ErrorAction Stop | Out-Null
    Get-ChildItem -LiteralPath $projectPath -Force |
        Where-Object { $_.Name -notin @(".pio", ".git", "dist") } |
        ForEach-Object {
            Copy-Item -LiteralPath $_.FullName -Destination $buildPathFull -Recurse -Force
        }
    Push-Location $buildPathFull
    try {
        foreach ($buildEnvironment in $Environment) {
        & $pioPath run -e $buildEnvironment
        if ($LASTEXITCODE -ne 0) {
            throw "PlatformIO build failed with exit code $LASTEXITCODE"
        }
        $sourceArtifacts = Join-Path $buildPathFull ".pio\build\$buildEnvironment"
        $destinationArtifacts = Join-Path $projectPath ".pio\build\$buildEnvironment"
        New-Item -ItemType Directory -Force -Path $destinationArtifacts | Out-Null
        Get-ChildItem -LiteralPath $sourceArtifacts -File |
            Where-Object { $_.Extension -in @(".bin", ".elf", ".map") } |
            ForEach-Object {
                Copy-Item -LiteralPath $_.FullName -Destination $destinationArtifacts -Force
            }
        }
    }
    finally {
        Pop-Location
    }
}
finally {
    if (Test-Path -LiteralPath $buildPathFull) {
        Remove-Item -LiteralPath $buildPathFull -Recurse -Force
    }
}
