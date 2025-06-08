cls

if not exist ".\build\" mkdir .\build\

clang -g -DDEBUG -DDEVELOPER_MODE=1 -Wno-writable-strings src/PlatformWin32DX11.cpp -o build/TowerDefense.exe