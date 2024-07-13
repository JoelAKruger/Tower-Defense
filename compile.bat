cls

if not exist ".\build\" mkdir .\build\

cl /D DEBUG=1 /ZI /EHsc /W3 /Febuild/Win32DX11.exe src\PlatformWin32DX11.cpp

cl /O2 /Febuild/Win32DX11Release.exe src\PlatformWin32DX11.cpp