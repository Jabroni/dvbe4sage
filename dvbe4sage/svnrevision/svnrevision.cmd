@echo off
setlocal enableextensions
for /f "delims=M " %%i in ('svnversion ..') do set REV=%%i
echo #define SVN_REVISION %REV% > 1.tmp
echo #define SVN_REVISION_TXT "%REV%" >> 1.tmp
if not exist svnrevision.h goto justCopy
fc 1.tmp svnrevision.h > nul 2>&1 && echo No revision change... || goto justCopy
goto end
:justCopy
copy /y 1.tmp svnrevision.h
:end
del 1.tmp
endlocal