# GGitar portable release packaging. No personal settings or user files are included.
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
$root = $PSScriptRoot
$cmake = Get-Content (Join-Path $root 'CMakeLists.txt') -Raw -Encoding UTF8
if ($cmake -notmatch 'project\(GGitar VERSION ([0-9.]+)') { throw 'Project version not found.' }
$version = $Matches[1]
$exe = Join-Path $root 'GGitar.exe'
if (-not (Test-Path $exe)) { throw 'GGitar.exe not found. Run build.bat first.' }
$out = Join-Path $root '发布'
New-Item -ItemType Directory -Force $out | Out-Null
$temp = Join-Path $out ([guid]::NewGuid().ToString('N'))
$stage = Join-Path $temp "GGitar-$version-windows-x64"
$source = Join-Path $temp "GGitar-$version-source"
$sourceZip = Join-Path $out "GGitar-$version-source.zip"
$appZip = Join-Path $out "GGitar-$version-windows-x64.zip"
try {
    New-Item -ItemType Directory -Force $stage, $source | Out-Null
    Copy-Item $exe (Join-Path $stage 'GGitar.exe')
    foreach ($name in @('README.md', 'LICENSE', '使用说明.txt', 'licenses')) {
        Copy-Item (Join-Path $root $name) (Join-Path $stage $name) -Recurse
    }
    foreach ($name in @('NAM', 'IR')) {
        New-Item -ItemType Directory -Force (Join-Path $stage "音色库\$name") | Out-Null
    }
    Set-Content (Join-Path $stage '音色库\README.txt') -Encoding UTF8 -Value 'Optional resources: add your licensed NAM models to NAM and WAV impulse responses to IR. No external models, images or user presets are bundled.'
    foreach ($name in @('Source', 'tests', 'docs', 'licenses', '.github', '.gitignore', 'CMakeLists.txt', 'build.bat', 'package.bat', 'package.ps1', 'README.md', 'LICENSE', '使用说明.txt')) {
        $item = Join-Path $root $name
        if (Test-Path $item) { Copy-Item $item (Join-Path $source $name) -Recurse }
    }
    New-Item -ItemType Directory (Join-Path $source 'third_party') | Out-Null
    foreach ($name in @('JUCE-9.0.4.zip', 'NAM-core.zip')) {
        Copy-Item (Join-Path $root "third_party\$name") (Join-Path $source "third_party\$name")
    }
    foreach ($zip in @($sourceZip, $appZip)) { if (Test-Path $zip) { Remove-Item $zip -Force } }
    [IO.Compression.ZipFile]::CreateFromDirectory($source, $sourceZip, [IO.Compression.CompressionLevel]::Optimal, $true, [Text.Encoding]::UTF8)
    Copy-Item $sourceZip (Join-Path $stage 'source.zip')
    [IO.Compression.ZipFile]::CreateFromDirectory($stage, $appZip, [IO.Compression.CompressionLevel]::Optimal, $true, [Text.Encoding]::UTF8)
    $checksums = foreach ($zip in @($appZip, $sourceZip)) {
        $hash = (Get-FileHash $zip -Algorithm SHA256).Hash.ToLowerInvariant()
        "$hash  $([IO.Path]::GetFileName($zip))"
    }
    Set-Content (Join-Path $out 'SHA256SUMS.txt') -Encoding ASCII -Value $checksums
    Write-Host "Created $appZip"
    Write-Host "Created $sourceZip"
}
finally {
    if (Test-Path $temp) { Remove-Item $temp -Recurse -Force }
}
