@echo off
rem Installs driver in device manager via devcon.
cd "x64\Win8Release\W83795GDriver Package"
devcon install W83795GDriver.inf root\W83795GDriver
cd "..\..\.."
