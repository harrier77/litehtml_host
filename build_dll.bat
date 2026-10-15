@echo off
rem ===========================================================================
rem  build_dll.bat - builds litehtml_c.dll (C++ side) with MinGW-w64.
rem
rem  gumbo is C99 -> compiled with gcc.
rem  litehtml + bridge are C++17 -> compiled with g++.
rem  (g++ would treat .c files as C++, so keep them separated.)
rem
rem  Output: litehtml_c.dll  (+ liblitehtml_c.a import lib, optional)
rem ===========================================================================
setlocal enabledelayedexpansion

set "MINGW=C:\Users\pr30565\Nim_code\mingw64\bin"
set "GCC=%MINGW%\gcc.exe"
set "GXX=%MINGW%\g++.exe"
set "LITEHTML=%~dp0litehtml"
set "HERE=%~dp0"
set "BRIDGE=%HERE%bridge"
set "OBJ=%HERE%obj"

if not exist "%GCC%" (
  echo [ERRORE] gcc non trovato in "%GCC%"
  exit /b 1
)
if not exist "%LITEHTML%\include\litehtml.h" (
  echo [ERRORE] Progetto litehtml non trovato in "%LITEHTML%"
  exit /b 1
)

if not exist "%OBJ%" mkdir "%OBJ%"
if not exist "%OBJ%\gumbo" mkdir "%OBJ%\gumbo"
if not exist "%OBJ%\litehtml" mkdir "%OBJ%\litehtml"

set "IFLAGS=-I"%LITEHTML%\include" -I"%LITEHTML%\include\litehtml" -I"%LITEHTML%\src" -I"%LITEHTML%\src\gumbo\src""

echo [1/3] Compilazione gumbo (C99) con gcc...
for %%f in ("%LITEHTML%\src\gumbo\src\*.c") do (
  "%GCC%" -O2 -c %IFLAGS% "%%f" -o "%OBJ%\gumbo\%%~nf.o" || goto :err
)

echo [2/3] Compilazione litehtml + bridge (C++17) con g++...
for %%f in ("%LITEHTML%\src\*.cpp") do (
  "%GXX%" -O2 -std=c++17 -fexceptions -c %IFLAGS% "%%f" -o "%OBJ%\litehtml\%%~nf.o" || goto :err
)
"%GXX%" -O2 -std=c++17 -fexceptions -c %IFLAGS% -I"%BRIDGE%" "%BRIDGE%\litehtml_c.cpp" -o "%OBJ%\litehtml_c.o" || goto :err

echo [3/3] Link litehtml_c.dll...
"%GXX%" -shared -o "%HERE%litehtml_c.dll" ^
  "%OBJ%\litehtml\*.o" "%OBJ%\gumbo\*.o" "%OBJ%\litehtml_c.o" ^
  "%BRIDGE%\litehtml_c.def" ^
  -static-libgcc -static-libstdc++ -Wl,--out-implib,"%HERE%liblitehtml_c.a" ^
  -lgdi32 -luser32 || goto :err

echo [OK] Creato "%HERE%litehtml_c.dll"
exit /b 0

:err
echo [ERRORE] Compilazione fallita.
exit /b 1
