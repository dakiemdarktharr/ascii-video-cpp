param(
    [Parameter(Mandatory=$true)][string]$PackageDir,
    [Parameter(Mandatory=$true)][string]$BuildDir,
    [string]$MingwPrefix = 'C:\msys64\mingw64'
)
$ErrorActionPreference = 'Stop'
$package = (Resolve-Path -LiteralPath $PackageDir).Path
$build = (Resolve-Path -LiteralPath $BuildDir).Path
$install = Join-Path $package 'installed smoke test'
$registration = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\ASCII Video C++'
if ((Test-Path -LiteralPath $install) -or (Test-Path -LiteralPath $registration)) {
    throw 'Smoke test needs a new destination and no existing registered installation.'
}
Add-Type -TypeDefinition @"
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class InstallerWindow {
    private delegate bool Callback(IntPtr window, IntPtr parameter);
    [DllImport("user32.dll")] private static extern bool EnumWindows(Callback callback, IntPtr parameter);
    [DllImport("user32.dll")] private static extern uint GetWindowThreadProcessId(IntPtr window, out uint process);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] private static extern int GetWindowText(IntPtr window, StringBuilder text, int count);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr window, uint message, IntPtr wParam, IntPtr lParam);
    public static IntPtr Find(uint process) {
        IntPtr result = IntPtr.Zero;
        EnumWindows((window, parameter) => {
            uint owner; GetWindowThreadProcessId(window, out owner);
            var title = new StringBuilder(256); GetWindowText(window, title, title.Capacity);
            if (owner == process && title.ToString() == "ascii-video-cpp") { result = window; return false; }
            return true;
        }, IntPtr.Zero);
        return result;
    }
}
"@
$oldPath = $env:PATH
$oldPlatform = $env:QT_QPA_PLATFORM
$oldPluginPath = $env:QT_PLUGIN_PATH
$oldFfmpeg = $env:ASCII_FFMPEG
try {
    $env:PATH = "$env:SystemRoot\System32;$env:SystemRoot"
    $env:QT_QPA_PLATFORM = $null
    $env:QT_PLUGIN_PATH = $null
    $env:ASCII_FFMPEG = $null
    $setup = Start-Process -FilePath (Join-Path $package 'Setup.exe') -ArgumentList "/S /D=$install" -PassThru -Wait -WindowStyle Hidden
    if ($setup.ExitCode -ne 0) { throw "Installer failed: $($setup.ExitCode)" }
    $exe = Join-Path $install 'ascii-video-cpp.exe'
    if (!(Test-Path -LiteralPath $exe) -or !(Test-Path -LiteralPath $registration)) {
        throw 'Installer did not create application and uninstall registration.'
    }
    $shortcut = Join-Path ([Environment]::GetFolderPath('Programs')) 'ASCII Video C++\ASCII Video C++.lnk'
    if (!(Test-Path -LiteralPath $shortcut)) { throw 'Start Menu shortcut is missing.' }
    $version = Join-Path $package 'installed-version.txt'
    $start = New-Object System.Diagnostics.ProcessStartInfo
    $start.FileName = $exe
    $start.Arguments = '--version'
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    $app = [System.Diagnostics.Process]::Start($start)
    if (!$app.WaitForExit(20000)) { $app.Kill(); throw 'Version command timed out.' }
    $versionText = $app.StandardOutput.ReadToEnd()
    $versionError = $app.StandardError.ReadToEnd()
    Set-Content -LiteralPath $version -Value $versionText
    if ($app.ExitCode -ne 0 -or $versionText -notmatch 'ascii-video-cpp 1\.1\.0') {
        throw "Installed app version check failed: exit $($app.ExitCode), $versionError"
    }
    $app.Dispose()
    $app = Start-Process -FilePath $exe -PassThru -WindowStyle Hidden
    try {
        $deadline = (Get-Date).AddSeconds(20)
        do {
            Start-Sleep -Milliseconds 200
            $app.Refresh()
            $window = [InstallerWindow]::Find($app.Id)
        } while (!$app.HasExited -and $window -eq [IntPtr]::Zero -and (Get-Date) -lt $deadline)
        if ($app.HasExited -or $window -eq [IntPtr]::Zero) { throw 'Installed Qt window failed to initialize.' }
        foreach ($module in $app.Modules) {
            if ($module.FileName.StartsWith($MingwPrefix, [StringComparison]::OrdinalIgnoreCase)) {
                throw "Runtime leaked from developer environment: $($module.ModuleName)"
            }
        }
    } finally {
        if (!$app.HasExited) {
            [void][InstallerWindow]::PostMessage($window, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)
            if (!$app.WaitForExit(10000)) { $app.Kill(); throw 'Application failed to close.' }
        }
    }
    # Run the real conversion/download/export tests against only the installed DLLs and FFmpeg.
    Copy-Item -LiteralPath (Join-Path $build 'ascii-tests.exe') -Destination $install
    Copy-Item -LiteralPath (Join-Path $MingwPrefix 'bin\Qt6Test.dll') -Destination $install
    $env:QT_QPA_PLATFORM = 'offscreen'
    Push-Location $install
    try {
        & .\ascii-tests.exe -o "$(Join-Path $package 'installed-tests.txt'),txt"
        if ($LASTEXITCODE -ne 0) { throw 'Installed runtime functional tests failed.' }
    } finally { Pop-Location }
    Remove-Item -LiteralPath (Join-Path $install 'ascii-tests.exe'), (Join-Path $install 'Qt6Test.dll')
    $marker = Join-Path $install 'user-export.txt'
    Set-Content -LiteralPath $marker -Value 'Uninstall must preserve files it does not own.'
    $uninstaller = Start-Process -FilePath (Join-Path $install 'Uninstall.exe') -ArgumentList '/S' -PassThru -Wait -WindowStyle Hidden
    if ($uninstaller.ExitCode -ne 0) { throw 'Uninstaller failed.' }
    $deadline = (Get-Date).AddSeconds(30)
    while ((Test-Path -LiteralPath $exe) -and (Get-Date) -lt $deadline) { Start-Sleep -Milliseconds 200 }
    if ((Test-Path -LiteralPath $exe) -or (Test-Path -LiteralPath $registration) -or (Test-Path -LiteralPath $shortcut)) {
        throw 'Uninstall left application files, registry entry or shortcut.'
    }
    if (!(Test-Path -LiteralPath $marker)) { throw 'Uninstall removed a user-owned file.' }
    Write-Output 'PASS: silent install, shortcut, GUI launch, isolated runtime tests and safe uninstall.'
} finally {
    $env:PATH = $oldPath
    $env:QT_QPA_PLATFORM = $oldPlatform
    $env:QT_PLUGIN_PATH = $oldPluginPath
    $env:ASCII_FFMPEG = $oldFfmpeg
}