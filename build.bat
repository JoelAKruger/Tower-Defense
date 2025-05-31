cls

if not exist ".\build\" mkdir .\build\

clang -g -DDEBUG -Wno-writable-strings src/PlatformWin32DX11.cpp -o build/TowerDefense.exe