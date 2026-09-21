@echo off
rem Builds and runs every Test.cpp in dsp; each test is linked with all other dsp\*.cpp modules.
rem Usage (from the repo root):  tools\run_tests.bat
setlocal EnableDelayedExpansion
if not exist out mkdir out
set FAILED=0
set "SRC="
for %%S in (dsp\*.cpp) do (
  set "NM=%%~nS"
  if /I not "!NM:~-4!"=="Test" set "SRC=!SRC! %%S"
)
for %%F in (dsp\*Test.cpp) do (
  set "T=%%~nF"
  set "N=!T:~0,-4!"
  echo == !N!
  g++ -std=c++17 -O0 -ffp-contract=off -Idsp !SRC! dsp\%%~nxF -o out\!N!_test.exe
  if errorlevel 1 (
    echo BUILD FAILED: !N!
    set /a FAILED+=1
  ) else (
    out\!N!_test.exe > out\!N!_test.txt
    if errorlevel 1 set /a FAILED+=1
    findstr /B "SUMMARY" out\!N!_test.txt
  )
)
if !FAILED!==0 (
  echo ALL TEST PROGRAMS PASSED
) else (
  echo !FAILED! test program^(s^) failed. Details: type out\NAME_test.txt ^| findstr FAIL
)
endlocal
