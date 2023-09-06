@echo off
SET arg=%1

:: Allow passing a debug flag for debug builds
SET pyflag=--enable-optimize
IF "%arg%"=="--debug" SET pyflag=--enable-debug

py ../configure.py --mms-path "C:/Users/johnm/Documents/GitHub/metamod-source" --sm-path "C:/Users/johnm/Documents/GitHub/sourcemod" --sdks none %pyflag%
ambuild
