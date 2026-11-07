@echo off
setlocal
rem [TEMP] Native x86 fixture for Bf2EditNode and HUD builder rows 67..72.
rem /FORCE:UNRESOLVED only covers the families the builder and HUD manager headers
rem pull in but these never call.
call "%~dp0..\Build\vsdevcmd.cmd" x86
set "TASK_ROOT=%~dp0..\.."
if not defined BF2_BUILD_OUT set "BF2_BUILD_OUT=%~dp0..\Build\obj_hud_edit_node_20260915"
if not exist "%BF2_BUILD_OUT%" mkdir "%BF2_BUILD_OUT%"
if not defined BF2_FIXTURE_OPT set "BF2_FIXTURE_OPT=/Od"
set "TASK_FLAGS=/nologo /c /MD /std:c++14 /permissive /EHsc %BF2_FIXTURE_OPT% /Z7 /Gy /DBF2_PLUGIN_ABI_NATIVE=1 /D_ITERATOR_DEBUG_LEVEL=0 /DDEDICATED=0"
set "TASK_OBJECTS="
for %%S in (Object TypeDescriptor Node System NamedObjectRegistry Data IntData FloatData Quad Stream OStream Effect ColorEffect EffectNode Style TransformNode Action ActionListAction ObjectList ObjectCollection CallFunctionAction Function) do (
  cl %TASK_FLAGS% /FI"%TASK_ROOT%\Code\BF2\Core\BuildConfig.h" "%TASK_ROOT%\Code\BF2\External\BFMeme\%%S.cpp" /Fo"%BF2_BUILD_OUT%\%%S.obj"
  if errorlevel 1 exit /b 1
  call set "TASK_OBJECTS=%%TASK_OBJECTS%% "%BF2_BUILD_OUT%\%%S.obj""
)
for %%S in (Debug StringUtil EventManager) do (
  cl %TASK_FLAGS% /FI"%TASK_ROOT%\Code\BF2\Core\BuildConfig.h" "%TASK_ROOT%\Code\BF2\Core\%%S.cpp" /Fo"%BF2_BUILD_OUT%\%%S.obj"
  if errorlevel 1 exit /b 1
  call set "TASK_OBJECTS=%%TASK_OBJECTS%% "%BF2_BUILD_OUT%\%%S.obj""
)
for %%S in (Bf2EditNode BfEditNode BfEditData Bf2NewListBoxNode BfNewListBoxNode BfListBoxData Bf2CallFunctionAction Bf2ActionListAction Bf2TransformNode BfTransformNode BF2HudManager HudBlinker HudColors) do (
  cl %TASK_FLAGS% /FI"%TASK_ROOT%\Code\BF2\Core\BuildConfig.h" "%TASK_ROOT%\Code\BF2\Menu\%%S.cpp" /Fo"%BF2_BUILD_OUT%\%%S.obj"
  if errorlevel 1 exit /b 1
  call set "TASK_OBJECTS=%%TASK_OBJECTS%% "%BF2_BUILD_OUT%\%%S.obj""
)
cl %TASK_FLAGS% /FI"%TASK_ROOT%\Code\BF2\Core\BuildConfig.h" "%TASK_ROOT%\Code\BF2\Game\HudInformationLayer.cpp" /Fo"%BF2_BUILD_OUT%\HudInformationLayer.obj"
if errorlevel 1 exit /b 1
call set "TASK_OBJECTS=%%TASK_OBJECTS%% "%BF2_BUILD_OUT%\HudInformationLayer.obj""
cl %TASK_FLAGS% /FI"%TASK_ROOT%\Code\BF2\Core\BuildConfig.h" "%~dp0hud_edit_node_runtime.cpp" /Fo"%BF2_BUILD_OUT%\fixture.obj"
if errorlevel 1 exit /b 1
link /nologo /FORCE:UNRESOLVED /OPT:NOREF /INCREMENTAL:NO /DEBUG /SUBSYSTEM:CONSOLE /OUT:"%BF2_BUILD_OUT%\hud_edit_node.exe" /MAP:"%BF2_BUILD_OUT%\hud_edit_node.map" %TASK_OBJECTS% "%BF2_BUILD_OUT%\fixture.obj" >"%BF2_BUILD_OUT%\link.log" 2>&1
if not exist "%BF2_BUILD_OUT%\hud_edit_node.exe" (
  type "%BF2_BUILD_OUT%\link.log"
  exit /b 1
)
"%BF2_BUILD_OUT%\hud_edit_node.exe"
if not "%errorlevel%"=="0" exit /b 1
call :death create-death    "FIXTURE_DEBUG line=856 Node: Warn, already exists!!"           FIXTURE_CREATE_RETURNED
if errorlevel 1 exit /b 1
call :death data-death      "FIXTURE_DEBUG line=878 Failed setting Data on node: Absent"     FIXTURE_DATA_RETURNED
if errorlevel 1 exit /b 1
call :death string-death    "FIXTURE_DEBUG line=894 Failed setting string on node: Absent"   FIXTURE_STRING_RETURNED
if errorlevel 1 exit /b 1
call :death maxlength-death "FIXTURE_DEBUG line=906 Failed max length on node: Absent"       FIXTURE_MAXLENGTH_RETURNED
if errorlevel 1 exit /b 1
call :death color-death     "FIXTURE_DEBUG line=918 Failed set color on node: Absent"        FIXTURE_COLOR_RETURNED
if errorlevel 1 exit /b 1
call :death font-death      "FIXTURE_DEBUG line=938 Failed setting font on node: Absent"     FIXTURE_FONT_RETURNED
if errorlevel 1 exit /b 1
echo HUD edit node and its six rows passed
exit /b 0

:death
"%BF2_BUILD_OUT%\hud_edit_node.exe" %1 >"%BF2_BUILD_OUT%\%1.log" 2>&1
if "%errorlevel%"=="0" (echo %1 did not stop & exit /b 1)
findstr /c:%2 "%BF2_BUILD_OUT%\%1.log" >nul
if errorlevel 1 (echo %1 missing its diagnostic & type "%BF2_BUILD_OUT%\%1.log" & exit /b 1)
findstr /c:"%3" "%BF2_BUILD_OUT%\%1.log" >nul
if not errorlevel 1 (echo %1 returned past its stop & exit /b 1)
echo %1: named diagnostic confirmed
exit /b 0
