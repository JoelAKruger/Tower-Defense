cls

if not exist ".\build\" mkdir .\build\

clang -g -DDEBUG -DDEVELOPER_MODE=1 -Wno-writable-strings src/Game.cpp -o build/TowerDefense.exe