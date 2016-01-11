@echo off
rem Create & install a certificate for driver.
cd "x64\Win8Release\W83795GDriver Package"
Makecert -r -pe -ss OpenSourceDriverCert OpenSourceDriverCert.cer
certmgr.exe -add OpenSourceDriverCert.cer -s -r localMachine root
certmgr.exe -add OpenSourceDriverCert.cer -s -r localMachine trustedpublisher
cd "..\..\.."
