param([string]$BuildDir = "build", [string]$AssetsDir = "assets")
$ErrorActionPreference = "Stop"
$generator = Join-Path $BuildDir "ascii-benchmark.exe"
if (-not (Test-Path -LiteralPath $generator)) {
    $generator = Join-Path $BuildDir "Release/ascii-benchmark.exe"
}
if (-not (Test-Path -LiteralPath $generator)) {
    throw "Build ascii-benchmark first (ASCII_BUILD_BENCHMARKS=ON)."
}
& $generator --demo $AssetsDir
if ($LASTEXITCODE -ne 0) { throw "Demo generation failed." }
