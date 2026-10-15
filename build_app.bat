@echo off
rem ===========================================================================
rem  build_app.bat - compila la demo C (demo.c) con TCC e la collega alla DLL.
rem
rem  Uso: build_app.bat [run]
rem ===========================================================================
setlocal

set "TCC=C:\Program Files (x86)\tcc64\tcc.exe"
set "DIR=%~dp0"
set "EXE=%DIR%demo.exe"

if not exist "%TCC%" (
  echo [ERRORE] tcc.exe non trovato in "%TCC%"
  exit /b 1
)
if not exist "%DIR%litehtml_c.dll" (
  echo [ERRORE] litehtml_c.dll mancante. Esegui prima build_dll.bat
  exit /b 1
)

echo Compilazione demo con tcc...
"%TCC%" "%DIR%demo.c" "%DIR%bridge\litehtml_c.def" -o "%EXE%" -I"%DIR%" -luser32 -lgdi32 -Wall
if errorlevel 1 (
  echo [ERRORE] Compilazione fallita.
  exit /b 1
)

echo [OK] Creato "%EXE%"
if /i "%~1"=="run" (
  echo Esecuzione...
  "%EXE%"
)
