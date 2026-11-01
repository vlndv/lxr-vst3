@echo off
rem Portable entry point. Run from any directory; pass Python runner options as arguments.
rem Creates fresh outputs and returns nonzero for build or test failures.
setlocal
if not defined LXR_PYTHON set "LXR_PYTHON=python"
"%LXR_PYTHON%" "%~dp0verification\run_checkpoint_tests.py" --suite native --output "%~dp0..\out\native-checkpoint-%RANDOM%-%RANDOM%" %*
exit /b %errorlevel%
