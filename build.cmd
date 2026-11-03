@echo off
setlocal
cd /d "%~dp0"

set CSC=%WINDIR%\Microsoft.NET\Framework64\v4.0.30319\csc.exe
if not exist "%CSC%" set CSC=%WINDIR%\Microsoft.NET\Framework\v4.0.30319\csc.exe
if not exist "%CSC%" (
  echo csc.exe not found. .NET Framework 4.x is required ^(ships with Windows 10/11^).
  exit /b 1
)

if not exist build mkdir build
if not exist dist mkdir dist

echo [1/3] Generating icon...
"%CSC%" /nologo /target:exe /optimize+ /main:MakeIcon /out:build\MakeIcon.exe ^
  /r:System.dll /r:System.Drawing.dll /r:System.Windows.Forms.dll ^
  tools\MakeIcon.cs src\Pixel.cs src\Native.cs src\Skin.cs src\Settings.cs src\App.cs || exit /b 1
build\MakeIcon.exe build\app.ico || exit /b 1

echo [2/3] Compiling TwentyTwentyTwenty.exe...
"%CSC%" /nologo /target:winexe /optimize+ /platform:anycpu ^
  /out:dist\TwentyTwentyTwenty.exe ^
  /win32icon:build\app.ico ^
  /win32manifest:src\app.manifest ^
  /r:System.dll /r:System.Drawing.dll /r:System.Windows.Forms.dll ^
  src\*.cs || exit /b 1

echo [3/3] Installer...
set ISCC=
for %%P in ("%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe" "%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe" "%ProgramFiles%\Inno Setup 6\ISCC.exe") do (
  if exist %%P if not defined ISCC set "ISCC=%%~P"
)
if defined ISCC (
  "%ISCC%" /Q installer\setup.iss || exit /b 1
  echo   dist\TwentyTwentyTwenty-Setup.exe
) else (
  echo   Inno Setup 6 not found - skipped. Install it ^(winget install JRSoftware.InnoSetup^) to build the installer.
)

echo.
for %%F in (dist\TwentyTwentyTwenty.exe) do echo Portable exe: dist\TwentyTwentyTwenty.exe  (%%~zF bytes^)
endlocal
