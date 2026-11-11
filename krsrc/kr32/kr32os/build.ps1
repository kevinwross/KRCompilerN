#Requires -Version 5.1
<#
Build KR32OS. The default tool path is the debug host build; pass -Kr32 to
select another kr32 executable.
#>
param(
    [string]$Kr32 = "",
    [string]$Out = "",
    [string]$Opt = ""
)

$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $here

if (-not $Kr32) {
    $Kr32 = $env:KR32
}
if (-not $Kr32) {
    $candidates = @(
        (Join-Path $root 'build\windows-msvc\Debug\kr32.exe'),
        (Join-Path $root 'build\windows-msvc\Release\kr32.exe'),
        (Join-Path $root 'build\windows-ninja\kr32.exe')
    )
    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate) {
            $Kr32 = $candidate
            break
        }
    }
}
if (-not $Kr32) {
    $command = Get-Command kr32 -ErrorAction SilentlyContinue
    if ($command -ne $null) {
        $Kr32 = $command.Source
    }
}
if (-not $Kr32 -or -not (Test-Path -LiteralPath $Kr32)) {
    throw 'kr32 was not found; pass -Kr32 <path> or set KR32.'
}
$Kr32 = (Resolve-Path -LiteralPath $Kr32).Path
if (-not $Out) { $Out = $env:KR32OS_BUILD_DIR }
if (-not $Out) { $Out = Join-Path $here 'build\windows' }
if (-not $Opt) { $Opt = $env:KR32OS_OPT }
if (-not $Opt) { $Opt = '-O1' }
$build = $Out
$biosBase = '0xFFFF0000'
$kernelBase = '0x00020000'
$userBase = '0x00400000'
New-Item -ItemType Directory -Force -Path $build | Out-Null

function Invoke-Kr32 {
    param([string[]]$Arguments)
    & $Kr32 @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "kr32 $($Arguments -join ' ') failed with exit code $LASTEXITCODE"
    }
}

$bios = Join-Path $build 'bios.bin'
$biosObject = Join-Path $build 'bios.o'
$biosK32 = Join-Path $build 'bios.k32'
$biosSym = Join-Path $build 'bios.sym'
$kernelAssembly = Join-Path $build 'kernel.s'
$kernelObject = Join-Path $build 'kernel.o'
$consoleAssembly = Join-Path $build 'console.s'
$consoleObject = Join-Path $build 'console.o'
$shellAssembly = Join-Path $build 'shell.s'
$shellObject = Join-Path $build 'shell.o'
$timerAssembly = Join-Path $build 'timer.s'
$timerObject = Join-Path $build 'timer.o'
$trapsAssembly = Join-Path $build 'traps.s'
$trapsObject = Join-Path $build 'traps.o'
$trapStubsObject = Join-Path $build 'trap_stubs.o'
$userAssembly = Join-Path $build 'user.s'
$userObject = Join-Path $build 'user.o'
$memoryAssembly = Join-Path $build 'memory.s'
$memoryObject = Join-Path $build 'memory.o'
$processAssembly = Join-Path $build 'process.s'
$processObject = Join-Path $build 'process.o'
$syscallAssembly = Join-Path $build 'syscall.s'
$syscallObject = Join-Path $build 'syscall.o'
$userBridgeObject = Join-Path $build 'user_bridge.o'
$userCrtObject = Join-Path $build 'crt0.o'
$userSyscallObject = Join-Path $build 'user_syscall.o'
$userInitAssembly = Join-Path $build 'user_init.s'
$userInitObject = Join-Path $build 'user_init.o'
$userImage = Join-Path $build 'init.bin'
$initrd = Join-Path $build 'initrd.bin'
$initrdAssembly = Join-Path $build 'initrd.s'
$initrdObject = Join-Path $build 'initrd.o'
$entryObject = Join-Path $build 'entry.o'
$kernel = Join-Path $build 'kernel.bin'
$kernelK32 = Join-Path $build 'kernel.k32'
$kernelMap = Join-Path $build 'kernel.map'
$kernelSym = Join-Path $build 'kernel.sym'
$kernelMeta = Join-Path $build 'kernel.meta'

Invoke-Kr32 -Arguments @('as', (Join-Path $here 'bios\entry.S'), '--object', '-o', $biosObject)
Invoke-Kr32 -Arguments @('ld', $biosObject, '--flat', '--base', $biosBase, '--sym', $biosSym, '-o', $bios)
Invoke-Kr32 -Arguments @('ld', $biosObject, '--base', $biosBase, '-o', $biosK32)
Invoke-Kr32 -Arguments @('cc', (Join-Path $here 'kernel\main.c'), '--source-id', '1', $Opt, '-S', '-o', $kernelAssembly)
Invoke-Kr32 -Arguments @('as', $kernelAssembly, '--object', '-o', $kernelObject)
Invoke-Kr32 -Arguments @('cc', (Join-Path $here 'kernel\console.c'), '--source-id', '2', $Opt, '-S', '-o', $consoleAssembly)
Invoke-Kr32 -Arguments @('as', $consoleAssembly, '--object', '-o', $consoleObject)
Invoke-Kr32 -Arguments @('cc', (Join-Path $here 'kernel\shell.c'), '--source-id', '3', $Opt, '-S', '-o', $shellAssembly)
Invoke-Kr32 -Arguments @('as', $shellAssembly, '--object', '-o', $shellObject)
Invoke-Kr32 -Arguments @('cc', (Join-Path $here 'kernel\timer.c'), '--source-id', '4', $Opt, '-S', '-o', $timerAssembly)
Invoke-Kr32 -Arguments @('as', $timerAssembly, '--object', '-o', $timerObject)
Invoke-Kr32 -Arguments @('cc', (Join-Path $here 'kernel\traps.c'), '--source-id', '5', $Opt, '-S', '-o', $trapsAssembly)
Invoke-Kr32 -Arguments @('as', $trapsAssembly, '--object', '-o', $trapsObject)
Invoke-Kr32 -Arguments @('as', (Join-Path $here 'kernel\traps-asm.S'), '--object', '-o', $trapStubsObject)
Invoke-Kr32 -Arguments @('cc', (Join-Path $here 'kernel\user.c'), '--source-id', '6', $Opt, '-S', '-o', $userAssembly)
Invoke-Kr32 -Arguments @('as', $userAssembly, '--object', '-o', $userObject)
Invoke-Kr32 -Arguments @('cc', (Join-Path $here 'kernel\memory.c'), '--source-id', '9', $Opt, '-S', '-o', $memoryAssembly)
Invoke-Kr32 -Arguments @('as', $memoryAssembly, '--object', '-o', $memoryObject)
Invoke-Kr32 -Arguments @('cc', (Join-Path $here 'kernel\process.c'), '--source-id', '8', $Opt, '-S', '-o', $processAssembly)
Invoke-Kr32 -Arguments @('as', $processAssembly, '--object', '-o', $processObject)
Invoke-Kr32 -Arguments @('cc', (Join-Path $here 'kernel\syscall.c'), '--source-id', '7', $Opt, '-S', '-o', $syscallAssembly)
Invoke-Kr32 -Arguments @('as', $syscallAssembly, '--object', '-o', $syscallObject)
Invoke-Kr32 -Arguments @('as', (Join-Path $here 'kernel\user-asm.S'), '--object', '-o', $userBridgeObject)
Invoke-Kr32 -Arguments @('as', (Join-Path $here 'boot\entry.S'), '--object', '-o', $entryObject)

# Build the first user program.
Invoke-Kr32 -Arguments @('cc', (Join-Path $here 'user\init.c'), $Opt, '-S', '-o', $userInitAssembly)
Invoke-Kr32 -Arguments @('as', $userInitAssembly, '--object', '-o', $userInitObject)
Invoke-Kr32 -Arguments @('as', (Join-Path $here 'libc\crt0.S'), '--object', '-o', $userCrtObject)
Invoke-Kr32 -Arguments @('as', (Join-Path $here 'libc\syscall.S'), '--object', '-o', $userSyscallObject)
Invoke-Kr32 -Arguments @('ld', $userCrtObject, $userInitObject, $userSyscallObject,
                        '--flat', '--base', $userBase, '-o', $userImage)

$userFaultAssembly = Join-Path $build 'user_fault.s'
$userFaultObject = Join-Path $build 'user_fault.o'
$userFaultImage = Join-Path $build 'fault.bin'
$userWxAssembly = Join-Path $build 'user_wx.s'
$userWxObject = Join-Path $build 'user_wx.o'
$userWxImage = Join-Path $build 'wx.bin'
Invoke-Kr32 -Arguments @('cc', (Join-Path $here 'user\fault.c'), $Opt, '-S', '-o', $userFaultAssembly)
Invoke-Kr32 -Arguments @('as', $userFaultAssembly, '--object', '-o', $userFaultObject)
Invoke-Kr32 -Arguments @('ld', $userCrtObject, $userFaultObject, $userSyscallObject,
                        '--flat', '--base', $userBase, '-o', $userFaultImage)
Invoke-Kr32 -Arguments @('cc', (Join-Path $here 'user\wx.c'), $Opt, '-S', '-o', $userWxAssembly)
Invoke-Kr32 -Arguments @('as', $userWxAssembly, '--object', '-o', $userWxObject)
Invoke-Kr32 -Arguments @('ld', $userCrtObject, $userWxObject, $userSyscallObject,
                        '--flat', '--base', $userBase, '-o', $userWxImage)

# Build the K32I initrd with init and fault entries.
$userBytes = [System.IO.File]::ReadAllBytes($userImage)
$faultBytes = [System.IO.File]::ReadAllBytes($userFaultImage)
$wxBytes = [System.IO.File]::ReadAllBytes($userWxImage)
$initrdMagic = [uint32]0x4932334B
$initrdVersion = [uint32]1
$initrdHeaderSize = 12
$initrdEntrySize = 44
$initrdNameSize = 32
$initrdEntryOffsetOffset = 32
$initrdEntryOffsetSize = 36
$initrdEntryOffsetEntry = 40
$userCodeBase = [uint32]$userBase
$initrdEntryCount = 3
$payloadOffset = $initrdHeaderSize + $initrdEntryCount * $initrdEntrySize
$initrdBytes = New-Object byte[] ($payloadOffset + $userBytes.Length + $faultBytes.Length + $wxBytes.Length)
[System.Array]::Copy([System.BitConverter]::GetBytes($initrdMagic), 0, $initrdBytes, 0, 4)
[System.Array]::Copy([System.BitConverter]::GetBytes($initrdVersion), 0, $initrdBytes, 4, 4)
[System.Array]::Copy([System.BitConverter]::GetBytes([uint32]$initrdEntryCount), 0, $initrdBytes, 8, 4)
$entries = @(
    @{ Name = 'init'; Offset = $payloadOffset; Size = $userBytes.Length; Bytes = $userBytes },
    @{ Name = 'fault'; Offset = $payloadOffset + $userBytes.Length; Size = $faultBytes.Length; Bytes = $faultBytes },
    @{ Name = 'wx'; Offset = $payloadOffset + $userBytes.Length + $faultBytes.Length; Size = $wxBytes.Length; Bytes = $wxBytes }
)
for ($index = 0; $index -lt $initrdEntryCount; ++$index) {
    $entryOffset = $initrdHeaderSize + $index * $initrdEntrySize
    $nameBytes = [System.Text.Encoding]::ASCII.GetBytes($entries[$index].Name)
    [System.Array]::Copy($nameBytes, 0, $initrdBytes, $entryOffset, $nameBytes.Length)
    [System.Array]::Copy([System.BitConverter]::GetBytes([uint32]$entries[$index].Offset), 0, $initrdBytes, $entryOffset + $initrdEntryOffsetOffset, 4)
    [System.Array]::Copy([System.BitConverter]::GetBytes([uint32]$entries[$index].Size), 0, $initrdBytes, $entryOffset + $initrdEntryOffsetSize, 4)
    [System.Array]::Copy([System.BitConverter]::GetBytes($userCodeBase), 0, $initrdBytes, $entryOffset + $initrdEntryOffsetEntry, 4)
    [System.Array]::Copy($entries[$index].Bytes, 0, $initrdBytes, $entries[$index].Offset, $entries[$index].Size)
}
[System.IO.File]::WriteAllBytes($initrd, $initrdBytes)
$initrdText = New-Object System.Text.StringBuilder
[void]$initrdText.AppendLine('.section .rodata')
[void]$initrdText.AppendLine('.global __initrd_start')
[void]$initrdText.AppendLine('__initrd_start:')
for ($index = 0; $index -lt $initrdBytes.Length; $index += 16) {
    $count = [Math]::Min(16, $initrdBytes.Length - $index)
    $parts = @()
    for ($offset = 0; $offset -lt $count; ++$offset) {
        $parts += ('0x{0:X2}' -f $initrdBytes[$index + $offset])
    }
    [void]$initrdText.AppendLine('.byte ' + ($parts -join ', '))
}
[void]$initrdText.AppendLine('.global __initrd_end')
[void]$initrdText.AppendLine('__initrd_end:')
Set-Content -LiteralPath $initrdAssembly -Value $initrdText.ToString() -NoNewline -Encoding ascii
Invoke-Kr32 -Arguments @('as', $initrdAssembly, '--object', '-o', $initrdObject)

Invoke-Kr32 -Arguments @('ld', $entryObject, $kernelObject, $consoleObject, $shellObject,
                        $timerObject, $trapsObject, $trapStubsObject,
                        $userObject, $syscallObject, $processObject, $memoryObject, $userBridgeObject, $initrdObject,
                        '--flat', '--base', $kernelBase, '--map', $kernelMap,
                        '--sym', $kernelSym, '-o', $kernel)
Invoke-Kr32 -Arguments @('ld', $entryObject, $kernelObject, $consoleObject, $shellObject,
                        $timerObject, $trapsObject, $trapStubsObject,
                        $userObject, $syscallObject, $processObject, $memoryObject, $userBridgeObject, $initrdObject,
                        '--base', $kernelBase, '-o', $kernelK32)

# Generate kernel.meta from the linker map and kernel image.
$mapLines = Get-Content -LiteralPath $kernelMap
$startLine = $mapLines | Where-Object { $_ -match '^\s*__initrd_start\s+(0x[0-9A-Fa-f]+)' } | Select-Object -First 1
$endLine = $mapLines | Where-Object { $_ -match '^\s*__initrd_end\s+(0x[0-9A-Fa-f]+)' } | Select-Object -First 1
if ($startLine -and $endLine) {
    [void]($startLine -match '0x([0-9A-Fa-f]+)')
    $initrdStart = [Convert]::ToUInt32($Matches[1], 16)
    [void]($endLine -match '0x([0-9A-Fa-f]+)')
    $initrdEnd = [Convert]::ToUInt32($Matches[1], 16)
    $imageBytes = [System.IO.File]::ReadAllBytes($kernel)
    [uint32]$crc = [uint32]::MaxValue
    foreach ($byte in $imageBytes) {
        $crc = $crc -bxor [uint32]$byte
        for ($bit = 0; $bit -lt 8; ++$bit) {
            if (($crc -band 1) -ne 0) {
                $crc = [uint32](($crc -shr 1) -bxor [uint32]3988292384)
            } else {
                $crc = [uint32]($crc -shr 1)
            }
        }
    }
    $crc = [uint32](-bnot $crc)
    $metaBytes = New-Object byte[] 24
    $metaWords = @(
        [uint32]0x4D32334B,
        [uint32]1,
        [uint32]0x00020000,
        [uint32]($initrdStart - 0x00020000),
        [uint32]($initrdEnd - $initrdStart),
        [uint32]$crc
    )
    for ($index = 0; $index -lt 6; ++$index) {
        [System.Array]::Copy([System.BitConverter]::GetBytes($metaWords[$index]), 0,
                             $metaBytes, $index * 4, 4)
    }
    [System.IO.File]::WriteAllBytes($kernelMeta, $metaBytes)
}

Write-Host "KR32OS M4 built:"
Write-Host "  $bios"
Write-Host "  $kernel"
