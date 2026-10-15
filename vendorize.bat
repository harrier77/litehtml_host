@echo off
rem ===========================================================================
rem  vendorize.bat - rende litehtml_host autosufficiente.
rem
rem  Copia dentro .\litehtml\ il sottoinsieme di sorgenti necessario a
rem  ricostruire litehtml_c.dll, cosi' non serve piu' il repository esterno.
rem
rem  Va eseguito UNA volta, con il repository litehtml presente in ..\litehtml.
rem  Dopo di che build_dll.bat usa la copia locale.
rem ===========================================================================
setlocal

set "SRC=%~dp0..\litehtml"
set "DST=%~dp0litehtml"

if not exist "%SRC%\include\litehtml.h" (
  echo [ERRORE] Repository litehtml non trovato in "%SRC%"
  exit /b 1
)

echo [1/5] include\ (header pubblici)...
robocopy "%SRC%\include" "%DST%\include" /E /NFL /NDL /NJH /NJS /NP >nul

echo [2/5] src\*.cpp + src\internal.h...
robocopy "%SRC%\src" "%DST%\src" *.cpp internal.h /NFL /NDL /NJH /NJS /NP >nul

echo [3/5] src\gumbo\src\ (parser HTML C99)...
robocopy "%SRC%\src\gumbo\src" "%DST%\src\gumbo\src" /E /NFL /NDL /NJH /NJS /NP >nul
robocopy "%SRC%\src\gumbo" "%DST%\src\gumbo" LICENSE /NFL /NDL /NJH /NJS /NP >nul

echo [4/5] Licenze...
copy /Y "%SRC%\LICENSE" "%DST%\LICENSE" >nul
if not exist "%DST%\src\gumbo\LICENSE" copy /Y "%SRC%\src\gumbo\LICENSE" "%DST%\src\gumbo\LICENSE" >nul

echo [5/5] Verifica...
set "OK=1"
if not exist "%DST%\include\litehtml.h" set "OK=0"
if not exist "%DST%\src\internal.h" set "OK=0"
if not exist "%DST%\src\gumbo\src\parser.c" set "OK=0"
if "%OK%"=="0" (
  echo [ERRORE] Copia incompleta.
  exit /b 1
)

echo [OK] Sorgenti litehtml copiati in "%DST%"
echo      build_dll.bat ora puo' usare la copia locale.
exit /b 0
