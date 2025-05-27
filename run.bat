@echo off 
setlocal
cd build
sdl_terminal.exe > runtime_log.txt 2>&1
endlocal