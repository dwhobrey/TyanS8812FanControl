Tyan S8812 Monitor, v1.0, 6th Feb 2013.

This package consists of four components for monitoring temperature 
and fan speeds on a Tyan S8812 set up as a workstation running
Microsoft Windows 8 x64.

W83795G Driver
This contains a driver for accessing the hardware monitor IC.
It also contains a program "cool.exe" for adjusting the
SMART IV fan profile duty cycle via calls to the driver.
Note: the driver needs to have a kernel CA certificate
when used outside of test mode.

See the ReadMe.htm file for further details.

The driver should be compiled and installed.
The cool.exe program should be run from startup via task scheduler.

IPMI Scanner
This contains a program to extract sensor data via the
Microsoft Generic IPMI driver via the WMI.
Data is written to a registry key for access by the IPMIMonitor sidebar gadget.
The scanner should be run from startup via task scheduler.

IPMI Monitor
This is a sidebar gadget to display the temperature and fan sensor values.

