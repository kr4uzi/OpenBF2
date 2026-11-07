# =================================================================================================
# ship.ps1 - THE SHIPPABLE RELEASE BUILD of bf2_w32ded.exe.
#
# SCAFFOLDING (a build script), not recovered source. Nothing under Code\BF2 is read for anything
# but compilation, and nothing under Code\BF2 is written.
#
#   Code\Tests\syntaxcheck.cmd   parses only (/Zs)            - does it read as C++?
#   Code\Build\build.cmd         DIAGNOSTIC compile + link    - which symbols are still unwritten?
#   Code\Build\bootrun.cmd       the bounded HARNESS image    - which gap does a boot hit first?
#   Code\Build\ship.cmd          THIS. The production image   - what would you hand to someone?
#
# WHY IT EXISTS. Every other build in this tree is a measuring instrument and is built like one:
#   * build.cmd compiles with NO /O flag at all (so, /Od) and links a diagnostic exe;
#   * bootrun.cmd links Code\Build\bootmain.cpp - bounded frames, object probes, a DbgHelp fault
#     handler - with /DEBUG and /MAP;
#   * the CMake bf2_w32ded target hard-codes /Zi in bf2_configure_cpp_target and /DEBUG + /MAP in
#     its target_link_options so CLion can step through it.
# All three put the reconstruction's own symbol names, source paths and line numbers INSIDE the
# image and drop a .pdb and .map beside it. This script produces the opposite: an optimised
# /SUBSYSTEM:CONSOLE image with no PDB, no CodeView debug entry, no map beside it and no assert
# strings - roughly the amount the retail bf2_w32ded.exe gives away.
#
# ** THE MANIFEST IS NOT DUPLICATED HERE. ** The engine image deliberately excludes translation
# units that belong to AIDLL_w32ded.dll or to BF2.exe, and that list lives in build.cmd, one
# `set "ENGINE_OBJECT=0"` line per file. This script PARSES those lines out of build.cmd instead of
# keeping a second copy, so a file added to or removed from the exclusion list cannot make the
# shipped image and the diagnostic image disagree. If the parse ever finds zero exclusions it stops
# rather than silently linking AIDLL objects into the server.
#
# ** WHAT IS DELIBERATELY KEPT PRIVATE. ** /MAP does not change a single byte of the image - the
# linker writes it as a side file - so the map is written to the OBJECT directory, not the staging
# directory. That keeps a full symbol->RVA table for triaging a crash report from the tester while
# shipping an image with nothing in it. /DYNAMICBASE:NO is set for the same reason and matches
# retail: the image always loads at 0x400000, so a faulting address a tester pastes back maps
# straight onto that map instead of changing every run.
#
# ** /FORCE IS STILL ON, AND THAT IS NOT COSMETIC. ** The reconstruction is not link-complete. Every
# unwritten function resolves to RVA 0 and faults if the running server reaches it. This script
# prints how many such symbols the shipped image contains; it is not a claim that the server works.
# =================================================================================================
param(
    [string]$ObjDir,
    [string]$StageDir,
    [switch]$SkipCompile,
    # -Reference builds the SAME sources and the SAME link with build.cmd's flags instead of the
    # release ones: no /O2 and no NDEBUG. It exists to answer one question honestly - the engine has
    # never been compiled optimised before, so "the shipped build behaves like the build we have been
    # testing" has to be MEASURED, by running the two images side by side, not assumed. It writes to
    # its own object and staging directories and never touches the shipped ones.
    [switch]$Reference,
    # Regexes for translation units to leave OUT of this image, matched against the full path. This
    # exists for the layout/offset probes agents drop into the tree - files whose whole purpose is to
    # FAIL compilation so the error text prints a struct offset. They define no engine symbol, but a
    # build that just swallowed their failure would be a build that silently ships with a missing
    # translation unit, which is exactly what this script refuses to do. So the skip is explicit,
    # named on the command line, and printed.
    [string[]]$Exclude = @()
)

$suffix = if ($Reference) { '_ref' } else { '' }
if (-not $ObjDir)   { $ObjDir   = Join-Path $PSScriptRoot ('obj_ship_release' + $suffix) }
if (-not $StageDir) { $StageDir = Join-Path $PSScriptRoot ('bin_ship' + $suffix) }

$ErrorActionPreference = 'Stop'
$sw = [Diagnostics.Stopwatch]::StartNew()

$Root       = (Resolve-Path (Join-Path $PSScriptRoot '..\BF2')).Path
$BuildCmd   = Join-Path $PSScriptRoot 'build.cmd'
$External   = Join-Path $Root 'External'
$PyRelease  = Join-Path $External 'python-embedded\OBJ\python\release'
$DicePyLib  = Join-Path $PyRelease 'dice_py.lib'
$DicePyDll  = Join-Path $PyRelease 'dice_py.dll'

if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    throw "cl.exe is not on PATH - run this through Code\Build\ship.cmd, which calls VsDevCmd."
}

# /Fo in a response file is written unquoted, so a space anywhere in these paths would split the
# argument. Fail loudly instead of producing objects in unexpected places.
foreach ($p in @($Root, $ObjDir, $StageDir)) {
    if ($p -match '\s') { throw "path contains a space, which this script's response files cannot carry: $p" }
}

# -------------------------------------------------------------------------------------------------
# THE FLAGS. Everything below matches Code\Build\build.cmd except for the four release changes,
# which are marked. Keeping the rest identical is what makes this the same engine, built harder.
# -------------------------------------------------------------------------------------------------
$PyInc  = @("/I$External\python-embedded\py-src-2.3.4\Include", "/I$External\python-embedded\py-src-2.3.4\PC")
$ZipInc = @("/I$External\zlib-1.2.3", "/I$External\zlib-1.2.3\contrib\minizip")

# RELEASE CHANGE 1: /O2 (build.cmd passes no /O flag, i.e. /Od). /Gy so /OPT:ICF has comdats to fold.
# RELEASE CHANGE 2: /DNDEBUG. It drops the 129 assert() call sites, and with them the
#   __FILE__ path and the assertion text each one bakes into the image.
# RELEASE CHANGE 3: no /Zi, no /FS, no /Fd - no PDB is produced at any stage of this build.
# RELEASE CHANGE 4: /MP, safe here precisely because there is no shared PDB to serialise on. Objects
#   mirror the source tree, so the flat-directory basename collision that forced build.cmd to number
#   its objects cannot happen.
# -Reference drops exactly the two release switches and nothing else, so an A/B run isolates them.
$Rel = if ($Reference) { @() } else { @('/O2','/Gy','/DNDEBUG') }

$Cxx = @('/nologo','/c','/MP','/MD','/std:c++14','/permissive','/EHsc','/W3','/wd4996') + $Rel +
       @('/DDEDICATED=1',"/FI$Root\Core\BuildConfig.h") + $PyInc + $ZipInc

# The vendored C libraries. build.cmd gives zlib the zip include path and GameSpy none; same here.
$CZlib   = @('/nologo','/c','/MP','/MD','/W3','/wd4996') + $Rel +
           @('/DDEDICATED=1',"/FI$Root\Core\BuildConfig.h") + $ZipInc
$CPlain  = @('/nologo','/c','/MP','/MD','/W3','/wd4996') + $Rel +
           @('/DDEDICATED=1',"/FI$Root\Core\BuildConfig.h")

# -------------------------------------------------------------------------------------------------
# THE MANIFEST, parsed out of build.cmd (see the header). One line per excluded translation unit:
#     if /i "%%~fF"=="%ROOT%\Core\DllModule.cpp" set "ENGINE_OBJECT=0"
# -------------------------------------------------------------------------------------------------
$excluded = @(Get-Content $BuildCmd |
    Where-Object { $_ -match 'ENGINE_OBJECT=0' } |
    ForEach-Object { if ($_ -match '"%ROOT%(?<p>[^"]+)"') { (Join-Path $Root $matches['p'].TrimStart('\')) } })

if ($excluded.Count -lt 50) {
    throw "parsed only $($excluded.Count) image exclusions out of build.cmd - the manifest format changed. Refusing to link AIDLL/client objects into the server."
}
$excludedSet = [System.Collections.Generic.HashSet[string]]::new(
    [string[]]$excluded, [System.StringComparer]::OrdinalIgnoreCase)

$missing = $excluded | Where-Object { -not (Test-Path $_) }
if ($missing) { Write-Host "  note: $($missing.Count) exclusion entr(y/ies) no longer exist on disk (stale rows in build.cmd)" }

# build.cmd walks Code\BF2 for *.cpp and skips any path containing \External\; the third-party sets
# below are then listed explicitly. Reproduce exactly that.
$engineSources = @(Get-ChildItem -Path $Root -Recurse -Filter *.cpp -File |
    Where-Object { $_.FullName -notmatch '\\External\\' -and -not $excludedSet.Contains($_.FullName) } |
    ForEach-Object { $_.FullName })

if ($Exclude.Count -gt 0) {
    $dropped = @($engineSources | Where-Object { $f = $_; @($Exclude | Where-Object { $f -match $_ }).Count -gt 0 })
    $engineSources = @($engineSources | Where-Object { $dropped -notcontains $_ })
    foreach ($d in $dropped) { Write-Host ("   -Exclude drops: {0}" -f $d.Substring($Root.Length + 1)) }
    if ($dropped.Count -eq 0) { Write-Host ("   -Exclude matched nothing (patterns: {0})" -f ($Exclude -join ', ')) }
}

$zlibSources = @('adler32.c','crc32.c','inffast.c','inflate.c','inftrees.c','zutil.c',
                 'deflate.c','trees.c','compress.c','uncompr.c' |
    ForEach-Object { Join-Path $External "zlib-1.2.3\$_" }) +
               @('ioapi.c','unzip.c' |
    ForEach-Object { Join-Path $External "zlib-1.2.3\contrib\minizip\$_" })

$gsRoot = Join-Path $External 'GameSpy'
$gsSources = @('gcdkey\gcdkeys.c','darray.c','md5c.c','hashtable.c',
               'common\gsAvailable.c','common\gsPlatformUtil.c','common\gsCore.c','common\gsMemory.c',
               'common\gsDebug.c','common\gsAssert.c','common\gsPlatform.c','common\gsPlatformSocket.c',
               'common\gsCrypt.c','common\gsRC4.c','common\gsSHA1.c','common\gsLargeInt.c',
               'common\gsPlatformThread.c','common\gsXML.c','common\gsSSL.c','common\gsStringUtil.c',
               'gstats\gstats.c','gstats\gbucket.c',
               'qr2\qr2.c','qr2\qr2regkeys.c',
               'ghttp\ghttpBuffer.c','ghttp\ghttpCallbacks.c','ghttp\ghttpCommon.c',
               'ghttp\ghttpConnection.c','ghttp\ghttpEncryption.c','ghttp\ghttpMain.c',
               'ghttp\ghttpPost.c','ghttp\ghttpProcess.c',
               'natneg\natneg.c','natneg\NATify.c' |
    ForEach-Object { Join-Path $gsRoot $_ })

# TinyXML is C++ and takes the C++ flags. TIXML_USE_STL stays undefined - see build.cmd's note.
$tinySources = @('tinystr.cpp','tinyxml.cpp','tinyxmlerror.cpp','tinyxmlparser.cpp' |
    ForEach-Object { Join-Path $External "TinyXML\$_" })

$allMissing = @($zlibSources + $gsSources + $tinySources | Where-Object { -not (Test-Path $_) })
if ($allMissing) { throw "third-party source missing:`n  " + ($allMissing -join "`n  ") }

Write-Host ""
Write-Host $(if ($Reference) { "== REFERENCE build (build.cmd flags: no /O2, no NDEBUG) - for A/B only, NOT shippable ==" }
             else            { "== ship build ==" })
Write-Host ("   engine TUs : {0}   (of {1} .cpp under Code\BF2, {2} excluded by build.cmd's manifest)" -f `
    $engineSources.Count, ($engineSources.Count + $excluded.Count), $excluded.Count)
Write-Host ("   third party: {0} zlib/minizip + {1} GameSpy + {2} TinyXML" -f `
    $zlibSources.Count, $gsSources.Count, $tinySources.Count)
Write-Host "   objects    : $ObjDir"
Write-Host "   staging    : $StageDir"
Write-Host ""

# -------------------------------------------------------------------------------------------------
# COMPILE. One cl invocation per source directory, with /MP inside it. Objects mirror the source
# tree under $ObjDir, so two files with the same basename in different directories never collide.
# -------------------------------------------------------------------------------------------------
# NOTE: Windows PowerShell 5.1 wraps a native command's stderr in ErrorRecords when it is captured
# with 2>&1, and under $ErrorActionPreference = 'Stop' that turns an ordinary compiler warning into a
# terminating error. The compile and link steps therefore run with 'Continue' (set below) and are
# judged by $LASTEXITCODE / the produced file instead.
function Invoke-Group {
    param([string[]]$Sources, [string[]]$Flags, [string]$Label)

    $failed = 0
    $ok     = 0
    $groups = @($Sources | Group-Object -Property { Split-Path $_ -Parent })
    $i      = 0

    foreach ($g in $groups) {
        $i++
        $rel    = $g.Name.Substring($Root.Length).TrimStart('\')
        $outDir = if ($rel) { Join-Path $ObjDir $rel } else { $ObjDir }
        New-Item -ItemType Directory -Force -Path $outDir | Out-Null

        $rsp = Join-Path $outDir '_cl.rsp'
        $lines = @($Flags) + @("/Fo$outDir\") + @($g.Group | ForEach-Object { "`"$_`"" })
        Set-Content -Path $rsp -Value $lines -Encoding ASCII

        Write-Host ("   [{0,3}/{1,3}] {2,-58} {3,3} file(s)" -f $i, $groups.Count, ($(if ($rel) { $rel } else { '.' })), $g.Count)
        $log = & cl.exe "@$rsp" 2>&1
        if ($LASTEXITCODE -ne 0) {
            $errs = $log | Where-Object { $_ -match ': (error|fatal error) ' }
            $failed += $errs.Count
            Write-Host "  COMPILE FAIL in $rel"
            $errs | Select-Object -First 12 | ForEach-Object { Write-Host "    $_" }
            Add-Content -Path (Join-Path $ObjDir 'compile_errors.log') -Value $log
        }
        $ok += (Get-ChildItem -Path $outDir -Filter *.obj -File | Measure-Object).Count
    }
    Write-Host ("   {0,-12} objects: {1}   errors: {2}" -f $Label, $ok, $failed)
    return $failed
}

$ErrorActionPreference = 'Continue'   # see the note above Invoke-Group

if (-not $SkipCompile) {
    if (Test-Path $ObjDir) { Remove-Item -Recurse -Force $ObjDir }
    New-Item -ItemType Directory -Force -Path $ObjDir | Out-Null

    $bad  = 0
    $bad += Invoke-Group -Sources $engineSources -Flags $Cxx    -Label 'engine'
    $bad += Invoke-Group -Sources $zlibSources   -Flags $CZlib  -Label 'zlib'
    $bad += Invoke-Group -Sources $gsSources     -Flags $CPlain -Label 'gamespy'
    $bad += Invoke-Group -Sources $tinySources   -Flags $Cxx    -Label 'tinyxml'

    if ($bad -gt 0) {
        Write-Host ""
        Write-Host "  $bad compile error(s) - see $ObjDir\compile_errors.log. NOT LINKING."
        exit 2
    }
}

# -------------------------------------------------------------------------------------------------
# LINK. No /DEBUG: the linker emits no PDB and writes no CodeView entry into the image, so the
# objects' debug records are discarded rather than published. /MAP goes to the OBJECT directory -
# it is a side file that does not touch the image, and it is the only way to read a crash address
# the tester sends back.
# -------------------------------------------------------------------------------------------------
New-Item -ItemType Directory -Force -Path $StageDir | Out-Null
$objs = @(Get-ChildItem -Path $ObjDir -Recurse -Filter *.obj -File | ForEach-Object { "`"$($_.FullName)`"" })
if ($objs.Count -eq 0) { throw "no objects in $ObjDir - run without -SkipCompile" }
$objRsp = Join-Path $ObjDir 'link_objects.rsp'
Set-Content -Path $objRsp -Value $objs -Encoding ASCII

$exe    = Join-Path $StageDir 'bf2_w32ded.exe'
$map    = Join-Path $ObjDir   'bf2_w32ded.map'
$linkLog= Join-Path $ObjDir   'shiplink.log'

$libs = @()
if (Test-Path $DicePyLib) { $libs += $DicePyLib }
else { Write-Host "  WARNING: $DicePyLib missing - every CPython call will be RVA 0. Run Code\Build\python\build_python.cmd." }
$libs += @('advapi32.lib','user32.lib','shell32.lib','Ws2_32.lib')

$linkArgs = @(
    '/nologo',
    '/FORCE',                      # the reconstruction is not link-complete; see the header
    '/SUBSYSTEM:CONSOLE',          # as retail, and as the production main in Game\Main\BF2.cpp
    '/NODEFAULTLIB:python23.lib',  # PC\pyconfig.h's #pragma comment(lib) - the real one is dice_py
    '/OPT:REF', '/OPT:ICF',        # the /DEBUG-less defaults, stated; ICF folding is retail-like
    '/DYNAMICBASE:NO',             # fixed 0x400000 base so a reported fault address is readable
    '/INCREMENTAL:NO',
    '/IGNORE:4006,4088,4098',
    "/MAP:$map",                   # PRIVATE - written to the object dir, never staged
    "/OUT:$exe",
    "@$objRsp"
) + $libs

if (Test-Path $exe) { Remove-Item -Force $exe }
& link.exe @linkArgs > $linkLog 2>&1
$linkRc = $LASTEXITCODE

if (-not (Test-Path $exe)) {
    Write-Host "  LINK PRODUCED NO IMAGE (rc=$linkRc) - see $linkLog"
    Get-Content $linkLog -Tail 25 | ForEach-Object { Write-Host "    $_" }
    exit 1
}

# The honest number: how many RVA-0 landmines /FORCE put in the shipped image.
# NOTE the shape of an MSVC line - the DEMANGLED prototype is quoted and the decorated name follows
# in parentheses, so splitting on whitespace counts leading keywords ("bool", "class", "void"...)
# instead of symbols. Count the decorated name.
$unresolved = @(Select-String -Path $linkLog -SimpleMatch 'unresolved external symbol')
$uniqueUnresolved = (@($unresolved | ForEach-Object {
        if ($_.Line -match 'unresolved external symbol\s+"?[^"]*"?\s*\((?<s>[^)]+)\)') { $matches['s'] }
        elseif ($_.Line -match 'unresolved external symbol\s+(?<t>[^\s(]+)')            { $matches['t'] } }) |
    Sort-Object -Unique).Count

# -------------------------------------------------------------------------------------------------
# VERIFY THE IMAGE IS ACTUALLY STRIPPED. Reading the PE directly rather than trusting the flags:
# data directory 6 is IMAGE_DIRECTORY_ENTRY_DEBUG, and a linker that emitted a PDB reference would
# put a CodeView record there with the .pdb path in it.
# -------------------------------------------------------------------------------------------------
$bytes    = [IO.File]::ReadAllBytes($exe)
$peOff    = [BitConverter]::ToInt32($bytes, 0x3C)
$optOff   = $peOff + 24
$magic    = [BitConverter]::ToUInt16($bytes, $optOff)
$dirBase  = $optOff + $(if ($magic -eq 0x20B) { 112 } else { 96 })
$dbgRva   = [BitConverter]::ToUInt32($bytes, $dirBase + 6*8)
$dbgSize  = [BitConverter]::ToUInt32($bytes, $dirBase + 6*8 + 4)
$imageBase= [BitConverter]::ToUInt32($bytes, $optOff + 28)

# Decode the debug directory's entry types (RVA -> file offset through the section table).
$nsec    = [BitConverter]::ToUInt16($bytes, $peOff + 6)
$optSize = [BitConverter]::ToUInt16($bytes, $peOff + 20)
$secBase = $peOff + 24 + $optSize
$dbgOff  = 0
for ($i = 0; $i -lt $nsec; $i++) {
    $s  = $secBase + $i * 40
    $va = [BitConverter]::ToUInt32($bytes, $s + 12)
    $vs = [BitConverter]::ToUInt32($bytes, $s + 8)
    if ($dbgRva -ge $va -and $dbgRva -lt ($va + [Math]::Max($vs, 1))) {
        $dbgOff = [BitConverter]::ToUInt32($bytes, $s + 20) + ($dbgRva - $va)
    }
}
$dbgKinds = @()
if ($dbgOff -gt 0) {
    for ($i = 0; $i -lt [int]($dbgSize / 28); $i++) {
        $t = [BitConverter]::ToUInt32($bytes, $dbgOff + $i * 28 + 12)
        $dbgKinds += switch ($t) { 1 {'COFF'} 2 {'CODEVIEW - A .PDB POINTER, THIS MUST NOT APPEAR'}
                                  12 {'VC_FEATURE'} 13 {'POGO'} 16 {'REPRO'} default {"type $t"} }
    }
}
$dbgKinds = if ($dbgKinds) { $dbgKinds -join ', ' } else { 'none' }

# A .pdb path anywhere in the file is the thing we are actually trying not to ship.
$ascii    = [Text.Encoding]::ASCII.GetString($bytes)
$pdbHits  = [regex]::Matches($ascii, '[A-Za-z]:\\[^\x00]{0,200}?\.pdb')
$strayPdb = @(Get-ChildItem -Path $StageDir -Filter *.pdb -File -ErrorAction SilentlyContinue)

# -------------------------------------------------------------------------------------------------
# STAGE. dice_py IS A REAL DLL: linking dice_py.lib is only half the job, and Windows searches the
# executable's own directory before the working directory. Same rule as bootrun.cmd and the CMake
# target - the exe and the DLL travel together.
# -------------------------------------------------------------------------------------------------
if (Test-Path $DicePyDll) { Copy-Item -Force $DicePyDll (Join-Path $StageDir 'dice_py.dll') }
else { Write-Host "  WARNING: $DicePyDll missing - the exe will not start (missing dice_py.dll)." }

# THE C RUNTIME HAS TO TRAVEL WITH THE IMAGE TOO, and this is not optional for a build that leaves
# this machine. /MD is mandatory here - dice_py.dll is a separate DLL and the two must share ONE CRT
# heap, which is why neither this script nor build.cmd may use /MT - and /MD means the image imports
# MSVCP140.dll and VCRUNTIME140.dll. On a machine with no Visual Studio and no VC++ redistributable
# those are simply absent, and the server dies with a missing-DLL box before main(). Retail had the
# same dependency and solved it the same way: MSVCR71.dll ships inside the BF2 install directory.
#
# The UCRT half (api-ms-win-crt-*.dll) is NOT copied - it is part of Windows 10 and 11 themselves.
# VCToolsRedistDir is set by VsDevCmd and points at the redistributable matching THIS toolset, so the
# staged DLLs can never be older than the compiler that produced the objects.
$redistDir = $env:VCToolsRedistDir
$crtStaged = 0
if ($redistDir -and (Test-Path (Join-Path $redistDir 'x86'))) {
    $crt = Get-ChildItem -Path (Join-Path $redistDir 'x86') -Directory -Filter 'Microsoft.VC*.CRT' |
           Sort-Object Name -Descending | Select-Object -First 1
    if ($crt) {
        # vcruntime140* and msvcp140* as a family: msvcp140.dll forwards a slice of the C++ library
        # to msvcp140_1/_2/_atomic_wait, so copying only the name in the import table is not enough.
        Get-ChildItem -Path $crt.FullName -File |
            Where-Object { $_.Name -match '^(vcruntime140|msvcp140)' } |
            ForEach-Object { Copy-Item -Force $_.FullName (Join-Path $StageDir $_.Name); $crtStaged++ }
    }
}
if ($crtStaged -eq 0) {
    Write-Host "  WARNING: no x86 VC++ runtime staged (VCToolsRedistDir='$redistDir'). The image will"
    Write-Host "           only start where the VC++ 2015-2022 x86 redistributable is already installed."
}

$sw.Stop()
$exeInfo = Get-Item $exe
Write-Host ""
Write-Host "== shipped image =="
Write-Host ("   {0}" -f $exe)
Write-Host ("   size            : {0:N0} bytes" -f $exeInfo.Length)
Write-Host ("   image base      : 0x{0:X8}   (ASLR off, so a faulting address is directly readable)" -f $imageBase)
# The debug directory is not expected to be empty: MSVC still emits a POGO/VC_FEATURE record, which
# carries section-layout counters and no path. What must NOT be there is a CODEVIEW entry (type 2),
# because that is the one holding the .pdb path - retail's own image leaks
# C:\dice\Projects\BF2Branches\Patch_1_50in\BF2_w32ded.pdb exactly that way. The .pdb-path scan
# below is the check that actually matters.
Write-Host ("   debug directory : RVA 0x{0:X} size {1}   ({2})" -f $dbgRva, $dbgSize, $dbgKinds)
Write-Host ("   .pdb paths in image : {0}   <- this is the one that must be 0" -f $pdbHits.Count)
if ($pdbHits.Count -gt 0) { $pdbHits | Select-Object -First 5 | ForEach-Object { Write-Host "      $($_.Value)" } }
Write-Host ("   stray .pdb staged   : {0}" -f $strayPdb.Count)
Write-Host ("   private map     : {0}" -f $map)
Write-Host ""
Write-Host ("   /FORCE unresolved externals in this image: {0} references, {1} distinct symbols" -f `
    $unresolved.Count, $uniqueUnresolved)
Write-Host ("   full list: {0}" -f $linkLog)
Write-Host ""
Get-ChildItem -Path $StageDir -File | ForEach-Object { Write-Host ("   staged: {0,-20} {1,12:N0} bytes" -f $_.Name, $_.Length) }
Write-Host ""
Write-Host ("== done in {0:N1} min ==" -f $sw.Elapsed.TotalMinutes)
exit 0
