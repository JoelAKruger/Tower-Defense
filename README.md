# Tower Defense Game
Tower defense game and engine written for Windows in C++

## Build Instructions (vcpkg and Ninja)

### 1. Installing vcpkg
The first step is to clone the vcpkg repository from GitHub. The repository contains scripts to acquire the vcpkg executable and a registry of curated open-source libraries maintained by the vcpkg community. To do this, run:\
```> git clone https://github.com/microsoft/vcpkg.git``` \
Now that you have cloned the vcpkg repository, navigate to the vcpkg directory and execute the bootstrap script: \
```> vcpkg\bootstrap-vcpkg.bat```
### 2. Generating project files
There are different options for telling cmake how to integrate with vcpkg; here we use CMAKE_TOOLCHAIN_FILE on the command line. \
```> cmake -S . -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=vcpkg\scripts\buildsystems\vcpkg.cmake```
### 3. Building the project
```
> cd build
> ninja
```
Run the resulting exe from the repository directory