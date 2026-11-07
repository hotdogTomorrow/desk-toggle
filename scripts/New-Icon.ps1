# Rebuild the original code-drawn icon using only Windows' built-in drawing library.
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$sizes = @(16, 24, 32, 48, 64, 256)
$images = @()
foreach ($size in $sizes) {
    $bitmap = [System.Drawing.Bitmap]::new($size, $size)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $graphics.Clear([System.Drawing.Color]::Transparent)
    $graphics.ScaleTransform($size / 32.0, $size / 32.0)
    $blue = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(255, 37, 99, 235))
    $white = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::White)
    $graphics.FillRectangle($blue, 1, 3, 30, 23)
    $graphics.FillRectangle($white, 3, 5, 26, 18)
    $graphics.FillRectangle($blue, 6, 8, 4, 4)
    $graphics.FillRectangle($blue, 6, 15, 4, 4)
    $graphics.FillRectangle($blue, 13, 8, 4, 4)
    $graphics.FillRectangle($blue, 13, 15, 4, 4)
    $graphics.FillRectangle($blue, 14, 26, 4, 3)
    $graphics.FillRectangle($blue, 9, 29, 14, 2)
    $stream = [System.IO.MemoryStream]::new()
    $bitmap.Save($stream, [System.Drawing.Imaging.ImageFormat]::Png)
    $images += ,$stream.ToArray()
    $stream.Dispose()
    $blue.Dispose()
    $white.Dispose()
    $graphics.Dispose()
    $bitmap.Dispose()
}
$destination = Join-Path $PSScriptRoot '..\assets\desk-toggle.ico'
$file = [System.IO.File]::Create($destination)
$writer = [System.IO.BinaryWriter]::new($file)
try {
    $writer.Write([UInt16]0)
    $writer.Write([UInt16]1)
    $writer.Write([UInt16]$sizes.Count)
    $offset = 6 + 16 * $sizes.Count
    for ($i = 0; $i -lt $sizes.Count; $i++) {
        $dimension = $sizes[$i] % 256
        $writer.Write([byte]$dimension)
        $writer.Write([byte]$dimension)
        $writer.Write([UInt16]0)
        $writer.Write([UInt16]1)
        $writer.Write([UInt16]32)
        $writer.Write([UInt32]$images[$i].Length)
        $writer.Write([UInt32]$offset)
        $offset += $images[$i].Length
    }
    foreach ($data in $images) { $writer.Write([byte[]]$data) }
} finally { $writer.Dispose() }
