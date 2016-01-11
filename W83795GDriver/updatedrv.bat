@echo off
rem Updates driver in device manager via devcon.
cd "x64\Win8Release\W83795GDriver Package"
devcon update W83795GDriver.inf root\W83795GDriver
cd "..\..\.."
