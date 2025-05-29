## Build Instructions

### 1. Prerequisites

- **Operating System:** Windows 10 or later
- **Compiler:** [Clang for Windows](https://releases.llvm.org/download.html)
- **Graphics:** A DirectX 11-capable GPU

#### Install Clang

If Clang is not already installed, download it from the [official LLVM downloads page](https://releases.llvm.org/download.html) or install it via `winget`:

```sh
winget install LLVM.LLVM
```

Make sure `clang.exe` is available in your system `PATH`.

> You can verify this by running `clang --version` in a command prompt.

---

### 2. Build the Project

From the root of the repository:

```bat
build.bat
```

This script does the following:
- Clears the terminal (`cls`)
- Creates a `build\` directory if it doesn't exist
- Compiles `src/PlatformWin32DX11.cpp` into `build/TowerDefense.exe` using Clang

### 3. Run the Game

After building, run the executable:

```sh
build\TowerDefense.exe
```

---

## Project Layout

```
/
├── build.bat            # Simple build script
├── build/               # Output directory (created automatically)
├── src/
│   └── PlatformWin32DX11.cpp  # Entry point
│   └── *.cpp            # All other code files
├── assets/              # Game assets (textures, models, etc.)
```

---

## Notes

- The build currently targets Windows with a DirectX 11 renderer.
- The `-DDEBUG` flag enables debug mode at compile time.