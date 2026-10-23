[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$fontSource = Join-Path $projectRoot 'tmp\msyh-regular.ttf'
$fontDirectory = Join-Path $projectRoot 'main\fonts'

if (-not (Test-Path -LiteralPath $fontSource)) {
    throw "Source font was not found: $fontSource"
}

# Do not maintain a second, hand-written character list. Extract every CJK and
# full-width punctuation code point used by the UI source. Passing code points
# through --range keeps this process independent from the Windows console code
# page, which previously caused some glyphs to be omitted.
$sourceFiles = Get-ChildItem -LiteralPath (Join-Path $projectRoot 'main') -File -Filter '*.c' |
    Where-Object { $_.DirectoryName -ne $fontDirectory }
$codePoints = [System.Collections.Generic.HashSet[int]]::new()
foreach ($sourceFile in $sourceFiles) {
    $content = Get-Content -LiteralPath $sourceFile.FullName -Raw -Encoding UTF8
    foreach ($character in $content.ToCharArray()) {
        $codePoint = [int][char]$character
        if (($codePoint -ge 0x3000 -and $codePoint -le 0x9FFF) -or
            ($codePoint -ge 0xFF00 -and $codePoint -le 0xFFEF)) {
            [void]$codePoints.Add($codePoint)
        }
    }
}

if ($codePoints.Count -eq 0) {
    throw 'No CJK glyphs were found in the UI source.'
}

$range = (($codePoints | Sort-Object | ForEach-Object { '0x{0:X}' -f $_ }) -join ',')
foreach ($size in @(16, 20)) {
    $fontName = "panel_zh_$size"
    $target = Join-Path $fontDirectory "$fontName.c"
    $temporaryTarget = "$target.generated"
    Remove-Item -LiteralPath $temporaryTarget -Force -ErrorAction SilentlyContinue

    & 'D:\Program Files\nodejs\npx.cmd' --yes lv_font_conv --size $size --bpp 4 --format lvgl `
        --font $fontSource --range $range --no-kerning --lv-include lvgl.h `
        --lv-font-name $fontName --lv-fallback "lv_font_montserrat_$size" -o $temporaryTarget
    if ($LASTEXITCODE -ne 0) {
        throw "LVGL font generation failed for $fontName."
    }
    Move-Item -LiteralPath $temporaryTarget -Destination $target -Force
}

Write-Host "Generated panel fonts from $($codePoints.Count) UI glyph code points."
