cls

if not exist ".\build\" mkdir .\build\

cl /Ilib\include /D DEBUG=1 /ZI /EHsc /Febuild/Win32DX11.exe src\PlatformWin32DX11.cpp /MDd /link
REM cl /O2 /Ilib\include /ZI /EHsc /Febuild/Win32DX11.exe src\PlatformWin32DX11.cpp /MDd /link
