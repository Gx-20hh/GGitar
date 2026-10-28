# GGitar 打包脚本（由 package.bat 调用）
# 生成：
#   发布\GGitar-v<版本>.zip       给别人用的：程序 + 音色库 + 你的音色预设和设置 + 使用说明 + 许可证 + 源码.zip
#   别人解压后第一次打开，看到的音色、效果器、鼓机和练习设置都和你打包时一样（声卡要他自己选）
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$cmake = Get-Content (Join-Path $root 'CMakeLists.txt') -Raw -Encoding UTF8
$ver = if ($cmake -match 'project\(GGitar VERSION ([0-9.]+)') { $Matches[1] } else { '0.0.0' }

$outDir  = Join-Path $root '发布'
$name    = "GGitar-v$ver"
$stage   = Join-Path $outDir $name
$tmp     = Join-Path $outDir '_tmp'
$zipFile = Join-Path $outDir "$name.zip"

foreach ($p in @($stage, $tmp, $zipFile)) { if (Test-Path $p) { Remove-Item $p -Recurse -Force } }
New-Item -ItemType Directory -Force $stage | Out-Null
New-Item -ItemType Directory -Force $tmp | Out-Null

Write-Host "[1/4] 复制程序和音色库..."
Copy-Item (Join-Path $root 'GGitar.exe') $stage
$lib = Join-Path $root '音色库'
if (Test-Path $lib) { Copy-Item $lib (Join-Path $stage '音色库') -Recurse }
else { New-Item -ItemType Directory (Join-Path $stage '音色库') | Out-Null }
$gvDir = Join-Path $root '音色库\Gervana'
$gvCount = if (Test-Path $gvDir) { (Get-ChildItem $gvDir -Include *.png,*.jpg -Recurse).Count } else { 0 }
if ($gvCount -lt 6) { Write-Host "  注意：音色库\Gervana 里只有 $gvCount 张效果器图片（应该有 6 张），别人那边会显示“图片待替换”" -ForegroundColor Yellow }

Write-Host "      你的音色预设和设置（让别人打开后和你看到的一样）..."
# 音色预设：我的文档\GGitar\音色预设
$myPresets = Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'GGitar\音色预设'
if (Test-Path $myPresets) {
    Copy-Item $myPresets (Join-Path $stage '音色预设') -Recurse
    Write-Host ("        音色预设 {0} 个" -f (Get-ChildItem (Join-Path $stage '音色预设') -Filter *.xml).Count)
}
# 设置：%APPDATA%\GGitar\GGitar.settings，去掉只对你这台电脑有效的项（声卡、插件路径、最近打开、延迟校准）
$mySettings = Join-Path $env:APPDATA 'GGitar\GGitar.settings'
if (Test-Path $mySettings) {
    [xml]$x = Get-Content $mySettings -Raw -Encoding UTF8
    $drop = @('songLibrary', 'backingTracks', 'audioDevice', 'guitarChannel', 'knownPlugins', 'tabRecent', 'tabLastFile', 'tabLastFolder',
              'tabWatchFolder', 'judgeCalibrationMs', 'defaultPresetsVersion', 'bundledPresetsStamp')
    foreach ($v in @($x.PROPERTIES.VALUE)) { if ($drop -contains $v.GetAttribute('name')) { [void]$x.PROPERTIES.RemoveChild($v) } }
    $setDir = New-Item -ItemType Directory (Join-Path $stage '默认设置')
    $x.Save((Join-Path $setDir.FullName 'GGitar.settings'))
    $cur = @($x.PROPERTIES.VALUE | Where-Object { $_.GetAttribute('name') -eq 'currentPreset' })
    if ($cur.Count -gt 0) { Write-Host ("        打开时的音色：{0}" -f $cur[0].GetAttribute('val')) }
}

$tabs = New-Item -ItemType Directory (Join-Path $stage '吉他谱')
Set-Content (Join-Path $tabs.FullName '把吉他谱放这里.txt') -Encoding UTF8 -Value @(
    '把 Guitar Pro 谱子（.gp3 .gp4 .gp5 .gpx .gp）放在这个文件夹，',
    '在“练习谱面”里点“打开谱子…”时会默认打开这里。也可以直接把谱子文件拖进程序窗口。')
Copy-Item (Join-Path $root '使用说明.txt') $stage
Copy-Item (Join-Path $root 'LICENSE') $stage
Copy-Item (Join-Path $root 'licenses') (Join-Path $stage '许可证') -Recurse

Write-Host "[2/4] 打包源码（AGPLv3 要求随程序提供源码）..."
$src = Join-Path $tmp 'GGitar-源码'
New-Item -ItemType Directory $src | Out-Null
foreach ($item in @('Source', 'tests', 'licenses', 'docs', 'CMakeLists.txt', 'build.bat', 'package.bat', 'package.ps1',
                    'README.md', 'LICENSE', '使用说明.txt', '.gitignore')) {
    $p = Join-Path $root $item
    if (Test-Path $p) { Copy-Item $p (Join-Path $src $item) -Recurse }
}
New-Item -ItemType Directory (Join-Path $src 'third_party') | Out-Null
foreach ($z in @('JUCE-9.0.4.zip', 'NAM-core.zip')) {
    $p = Join-Path $root "third_party\$z"
    if (Test-Path $p) { Copy-Item $p (Join-Path $src 'third_party') }
}
[IO.Compression.ZipFile]::CreateFromDirectory($src, (Join-Path $stage '源码.zip'),
    [IO.Compression.CompressionLevel]::Optimal, $true, [Text.Encoding]::UTF8)

Write-Host "[3/4] 生成压缩包..."
[IO.Compression.ZipFile]::CreateFromDirectory($stage, $zipFile,
    [IO.Compression.CompressionLevel]::Optimal, $true, [Text.Encoding]::UTF8)

Write-Host "[4/4] 清理临时文件..."
Remove-Item $tmp -Recurse -Force

$mb = [math]::Round((Get-Item $zipFile).Length / 1MB, 1)
Write-Host ""
Write-Host "生成：$zipFile （$mb MB）"
Write-Host "解压后的文件夹（可以直接试运行）：$stage"
