@echo off
setlocal
cd /d "%~dp0"
rem Kullanim: build.bat [x64|x86]   (varsayilan x86: daha kucuk exe)
set ARCH=%1
if "%ARCH%"=="" set ARCH=x86
call "C:\Program Files\Microsoft Visual Studio\18\Professional\VC\Auxiliary\Build\vcvarsall.bat" %ARCH% >nul 2>&1
cl /nologo /O1 /Os /GS- /Gy /Zl /W3 /EHs-c- /GR- /utf-8 shot.cpp /Fe:shot.exe /link /NODEFAULTLIB kernel32.lib user32.lib gdi32.lib shell32.lib advapi32.lib gdiplus.lib dwmapi.lib /OPT:REF /OPT:ICF /MERGE:.rdata=.text /MERGE:.data=.text /MERGE:.pdata=.text /SECTION:.text,ERW /FILEALIGN:512 /FIXED /DYNAMICBASE:NO /DEBUG:NONE /EMITTOOLVERSIONINFO:NO /MANIFEST:NO
del shot.obj 2>nul
for %%F in (shot.exe) do echo Boyut: %%~zF bayt
