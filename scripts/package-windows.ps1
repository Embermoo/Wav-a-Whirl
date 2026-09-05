param(
    [string]$QtRoot = 'C:\QT2\6.11.0\mingw_64',
    [string]$CompilerBin = 'C:\QT2\Tools\mingw1310_64\bin',
    [string]$BuildDirectory = 'build-stage1',
    [string]$OutputName = 'Wav-a-Whirl-1.1-windows-x64-test'
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$buildRoot = Join-Path $projectRoot $BuildDirectory
$packageRoot = Join-Path (Join-Path $projectRoot 'dist') $OutputName
if (Test-Path -LiteralPath $packageRoot) { throw 'Output already exists. Choose a new OutputName.' }
$env:PATH = "$CompilerBin;$QtRoot\bin;" + $env:PATH
New-Item -ItemType Directory -Path $packageRoot -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $buildRoot 'Main.exe') -Destination (Join-Path $packageRoot 'Wav-a-Whirl.exe')
& (Join-Path $QtRoot 'bin\windeployqt.exe') --release --compiler-runtime --no-translations (Join-Path $packageRoot 'Wav-a-Whirl.exe')
if ($LASTEXITCODE -ne 0) { throw 'Qt deployment failed.' }
Copy-Item -LiteralPath (Join-Path $projectRoot 'release-notes\START-HERE.txt'),(Join-Path $projectRoot 'release-notes\TESTING.md') -Destination $packageRoot
$notices = Join-Path $packageRoot 'dependency-notices'
New-Item -ItemType Directory -Path $notices | Out-Null
$qtLicenseRoot = Join-Path (Split-Path (Split-Path $QtRoot -Parent) -Parent) 'Licenses'
if (Test-Path $qtLicenseRoot) { Copy-Item -LiteralPath $qtLicenseRoot -Destination (Join-Path $notices 'Qt-installer-licenses') -Recurse }
foreach ($module in @('qtbase','qtmultimedia','qtsvg')) {
    Copy-Item -LiteralPath (Join-Path $QtRoot "sbom\$module-6.11.0.spdx.json") -Destination $notices
}
$archive = "$packageRoot.zip"
Copy-Item -LiteralPath (Join-Path $projectRoot 'LICENSE') -Destination $packageRoot
Compress-Archive -LiteralPath $packageRoot -DestinationPath $archive
(Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash + '  ' + (Split-Path $archive -Leaf) | Set-Content "$archive.sha256"
Write-Output $archive
# The source license is in the repository's LICENSE file; include it when publishing a binary release.
