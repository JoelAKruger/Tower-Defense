# Tower Defense Game
Tower defense game and engine written for Windows in C++

## Build Instructions
Note: Only x64 Windows builds are available

### 1. Installing the Required Tools
Microsoft C/C++ Build Tools are required for the Windows SDK and the MSVC compiler.

### 2. Build Environment
Building the game can be done from the command line.
Call 'vcvarsall.bat x64' from the Microsoft C/C++ Build Tools. This can be done by running 'x64 Native Tools Command Prompt For VS (year)'.

### 3. Building
Within this command prompt, navigate to the project directory and run the 'compile.bat' script. 
If compilation is successful, the executables in the build directory can be run from the project directory.\
For example '.\build\Win32DX11.exe'