@ECHO OFF
Set MINGW=C:\MinGW-w64
Set BIT=64

::### Set paths ###::
Set PATH=%MINGW%\bin;%PATH%

::### Compile ###::
mingw32-make.exe BIT=%BIT% all clean

::### End ###::
if not "%1" == "/S" @PAUSE
if not %ERRORLEVEL% == 0 exit %2 %ERRORLEVEL%
