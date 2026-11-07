param([ValidateSet('x64', 'ARM64')][string]$Architecture = 'x64')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$executable = Join-Path $root 'build\Release\DeskToggle.exe'
if (-not (Test-Path -LiteralPath $executable)) { throw 'Build Release before packaging.' }
$reader = [IO.BinaryReader]::new([IO.File]::OpenRead($executable))
try {
    $reader.BaseStream.Position = 0x3c
    $peOffset = $reader.ReadInt32()
    $reader.BaseStream.Position = $peOffset + 4
    $machine = $reader.ReadUInt16()
} finally { $reader.Dispose() }
$expected = if ($Architecture -eq 'x64') { 0x8664 } else { 0xaa64 }
if ($machine -ne $expected) { throw 'Executable architecture does not match the package name.' }
$destination = Join-Path $root "dist\package-$Architecture"
New-Item -ItemType Directory -Force -Path $destination | Out-Null
Copy-Item -LiteralPath $executable -Destination $destination
Copy-Item -LiteralPath (Join-Path $root 'README.md'), (Join-Path $root 'LICENSE') -Destination $destination
$standalone = Join-Path $root "dist\DeskToggle-$Architecture.exe"
Copy-Item -LiteralPath $executable -Destination $standalone
$archive = Join-Path $root "dist\DeskToggle-$Architecture.zip"
# Only these three files are packaged, even if the destination contains old files.
Compress-Archive -LiteralPath (Join-Path $destination 'DeskToggle.exe'),
    (Join-Path $destination 'README.md'), (Join-Path $destination 'LICENSE') -DestinationPath $archive -Force
$hash = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()
[IO.File]::WriteAllText("$archive.sha256", "$hash  DeskToggle-$Architecture.zip`n", [Text.UTF8Encoding]::new($false))
$exeHash = (Get-FileHash -LiteralPath $standalone -Algorithm SHA256).Hash.ToLowerInvariant()
[IO.File]::WriteAllText("$standalone.sha256", "$exeHash  DeskToggle-$Architecture.exe`n", [Text.UTF8Encoding]::new($false))
Write-Output $archive
