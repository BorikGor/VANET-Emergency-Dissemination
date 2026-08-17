@echo off
setlocal enabledelayedexpansion

set "LOG=metrics_batch_run.log"

echo ================================================== > "%LOG%"
echo Batch run started: %DATE% %TIME%>> "%LOG%"
echo Working dir: %CD%>> "%LOG%"
echo ==================================================>> "%LOG%"
echo.>> "%LOG%"

REM Prefer python, fallback to py launcher if needed
set "PY=python"
%PY% --version >nul 2>&1
if errorlevel 1 (
    set "PY=py"
)

for %%F in (*.txt) do (
    echo -------------------------------------------------->> "%LOG%"
    echo FILE: %%F>> "%LOG%"
    echo TIME: %DATE% %TIME%>> "%LOG%"
    echo CMD : %PY% metrics_parser.py "%%F">> "%LOG%"
    echo -------------------------------------------------->> "%LOG%"
    %PY% metrics_parser.py "%%F" >> "%LOG%" 2>&1
    echo.>> "%LOG%"
)

echo ==================================================>> "%LOG%"
echo Batch run finished: %DATE% %TIME%>> "%LOG%"
echo ==================================================>> "%LOG%"

echo Done. Log saved to: "%LOG%"
endlocal
