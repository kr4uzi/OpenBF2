@echo off
rem ===========================================================================
rem [TEMP] SCAFFOLDING. Builds the black-box Bink harness with MSVC
rem (docs\tracks\bink\README.md):
rem   bink_harness_retail.exe  x86, loads RAD's binkw32.dll by name (place a copy
rem                            of the retail DLL beside it; never commit it)
rem   bink_harness_port.exe    x64, linked with the stand-in (RendDX9Abi\
rem                            BinkFFmpeg.cpp + BinkAudioDirectSound.cpp) and the
rem                            vendored FFmpeg Bink decoders
rem
rem   usage:  bink_harness.cmd [<out dir>]   (default Code\Tests\movies\obj)
rem Both programs write their measurements to a directory given on their own
rem command line; game content and dumps stay out of the repository.
rem No labels after parenthesised blocks: this file is stored with LF endings.
rem ===========================================================================
setlocal
set "SRC=%~dp0"
for %%I in ("%~dp0..\..") do set "CODE=%%~fI"
set "OUT=%~dp0obj"
if not "%~1"=="" set "OUT=%~1"
if not exist "%OUT%\x86" mkdir "%OUT%\x86"
if not exist "%OUT%\x64" mkdir "%OUT%\x64"

setlocal
call "%CODE%\Build\vsdevcmd.cmd" x86
if errorlevel 1 exit /b 1
cl /nologo /O2 /MT /EHsc /std:c++17 /W3 /DBINK_HARNESS_RETAIL /D_CRT_SECURE_NO_WARNINGS ^
   "%SRC%bink_harness.cpp" /Fo"%OUT%\x86\\" /Fe"%OUT%\bink_harness_retail.exe" dsound.lib dxguid.lib winmm.lib
if errorlevel 1 exit /b 1
endlocal

setlocal
call "%CODE%\Build\vsdevcmd.cmd" x64
if errorlevel 1 exit /b 1
set "FFB=%CODE%\BF2\External\ffmpeg-bink"
cl /nologo /c /O2 /MT /std:c11 /W3 /wd4244 /wd4267 /wd4018 /wd4146 /D_CRT_SECURE_NO_WARNINGS ^
   /I"%FFB%\shim" "%FFB%\libavcodec\bink.c" "%FFB%\libavcodec\binkdsp.c" "%FFB%\libavcodec\binkaudio.c" ^
   "%FFB%\libavcodec\wma_freqs.c" "%FFB%\shim\ffbink_shim.c" /Fo"%OUT%\x64\\"
if errorlevel 1 exit /b 1
cl /nologo /O2 /MT /EHsc /std:c++17 /W3 /D_CRT_SECURE_NO_WARNINGS ^
   "%SRC%bink_harness.cpp" "%CODE%\RendDX9Abi\BinkFFmpeg.cpp" "%CODE%\RendDX9Abi\BinkAudioDirectSound.cpp" ^
   "%OUT%\x64\bink.obj" "%OUT%\x64\binkdsp.obj" "%OUT%\x64\binkaudio.obj" "%OUT%\x64\wma_freqs.obj" "%OUT%\x64\ffbink_shim.obj" ^
   /Fo"%OUT%\x64\\" /Fe"%OUT%\bink_harness_port.exe" dsound.lib dxguid.lib winmm.lib
if errorlevel 1 exit /b 1
endlocal

echo bink_harness.cmd: built "%OUT%\bink_harness_retail.exe" and "%OUT%\bink_harness_port.exe"
exit /b 0
