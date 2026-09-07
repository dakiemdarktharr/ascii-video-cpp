param(
    [Parameter(Mandatory=$true)][string]$ShortCsv,
    [Parameter(Mandatory=$true)][string]$LongCsv,
    [double]$MaxGrowthMiB = 32
)
$ErrorActionPreference = "Stop"
$short = @(Import-Csv -LiteralPath $ShortCsv)
$long = @(Import-Csv -LiteralPath $LongCsv)
if ($short.Count -lt 2 -or $long.Count -lt 2) { throw "Need at least two samples from each run." }
foreach ($column in @("parent_bytes", "children_bytes")) {
    $shortPeak = ($short | ForEach-Object { [double]$_.$column } | Measure-Object -Maximum).Maximum
    $longPeak = ($long | ForEach-Object { [double]$_.$column } | Measure-Object -Maximum).Maximum
    if ($shortPeak -le 0 -or $longPeak -le 0) { throw "Missing memory samples for $column" }
    $growth = ($longPeak - $shortPeak) / 1MB
    if ($growth -gt $MaxGrowthMiB) { throw "$column grew by $growth MiB, beyond $MaxGrowthMiB MiB." }
    Write-Output ("{0}: short={1:F2} MiB, long={2:F2} MiB, growth={3:F2} MiB" -f $column, ($shortPeak/1MB), ($longPeak/1MB), $growth)
}
