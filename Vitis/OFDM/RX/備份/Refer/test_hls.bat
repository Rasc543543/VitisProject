@echo off
cd /d %~dp0
echo CWD: %CD%
echo Running vitis_hls...
C:\Xilinx\Vitis_HLS\2022.2\bin\vitis_hls.bat -f scripts\run_all.tcl
echo Exit code: %ERRORLEVEL%
pause
