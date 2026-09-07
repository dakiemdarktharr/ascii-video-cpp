param(
    [Parameter(Mandatory=$true)][string]$InputFile,
    [string]$BuildDir = "build",
    [string]$OutputFile = "build/memory-output.mp4",
    [string]$CsvFile = "build/memory-samples.csv"
)
$ErrorActionPreference = "Stop"
$exe = (Resolve-Path (Join-Path $BuildDir "ascii-benchmark.exe")).Path
$inputPath = (Resolve-Path -LiteralPath $InputFile).Path
$outputPath = [IO.Path]::GetFullPath($OutputFile)
$env:QT_QPA_PLATFORM = "offscreen"
$argumentList = '"' + $inputPath + '" "' + $outputPath + '" 4 100'
$process = Start-Process -FilePath $exe -ArgumentList $argumentList -PassThru -WindowStyle Hidden -RedirectStandardOutput "$OutputFile.json" -RedirectStandardError "$OutputFile.stderr"
$clock = [Diagnostics.Stopwatch]::StartNew()
"elapsed_ms,parent_bytes,children_bytes" | Set-Content -LiteralPath $CsvFile
while (-not $process.HasExited) {
    $process.Refresh()
    $children = @(Get-CimInstance Win32_Process -Filter "ParentProcessId=$($process.Id)" | ForEach-Object {
        Get-Process -Id $_.ProcessId -ErrorAction SilentlyContinue
    })
    $childBytes = ($children | Measure-Object WorkingSet64 -Sum).Sum
    "$($clock.ElapsedMilliseconds),$($process.WorkingSet64),$childBytes" | Add-Content -LiteralPath $CsvFile
    Start-Sleep -Milliseconds 200
}
$process.WaitForExit()
if ($process.ExitCode -ne 0) { throw "Benchmark failed; inspect $OutputFile.stderr" }
