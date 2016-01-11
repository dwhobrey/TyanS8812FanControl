@echo off
rem generates inf catalogue file and signs it.
cd "x64\Win8Release\W83795GDriver Package"
stampinf -f W83795GDriver.inf -d 01/10/2013 -v 1.0.0.0
inf2cat /driver:. /os:7_X64
signtool sign /s OpenSourceDriverCert W83795GDriver.cat
signtool verify /pa /v W83795GDriver.cat
cd "..\..\.."
