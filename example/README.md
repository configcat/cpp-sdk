# ConfigCat C++ Console Sample App

This is a simple application to demonstrate how to use the ConfigCat C++ SDK in a console application.

## Table of Contents

- [Instructions for CMake](#instructions-for-cmake)
- [Instructions for CLion](#instructions-for-clion)
- [Instructions for Visual Studio](#instructions-for-visual-studio)

## Instructions for CMake

### 1. Install [CMake](https://cmake.org/)

- Follow the instructions on https://cmake.org/install 

### 2. Install [Vcpkg](https://github.com/microsoft/vcpkg)

- On Windows:
  ```cmd
  git clone https://github.com/microsoft/vcpkg
  .\vcpkg\bootstrap-vcpkg.bat
  ```

- On Linux/Mac:
  ```bash
  git clone https://github.com/microsoft/vcpkg
  ./vcpkg/bootstrap-vcpkg.sh
  ```

### 3. Build with CMake

```bash
  cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=[path to vcpkg]/scripts/buildsystems/vcpkg.cmake
  cmake --build build
```

### 4. Run
- On Windows:
  ```cmd
  .\build\example.exe
  ```

- On Linux/Mac:
  ```bash
  ./build/example
  ```

## Instructions for CLion

### 1. Install [Vcpkg](https://github.com/microsoft/vcpkg)

- On Windows:
  ```cmd
  git clone https://github.com/microsoft/vcpkg
  .\vcpkg\bootstrap-vcpkg.bat
  ```

- On Linux/Mac:
  ```bash
  git clone https://github.com/microsoft/vcpkg
  ./vcpkg/bootstrap-vcpkg.sh
  ```

### 2. Open the example folder with CLion 

### 3. Update CLion Toolchain

Open the Toolchains settings
(File > Settings on Windows and Linux, CLion > Preferences on macOS),
and go to the CMake settings (Build, Execution, Deployment > CMake).
Finally, in `CMake options`, add the following line:

```
-DCMAKE_TOOLCHAIN_FILE=[vcpkg root]/scripts/buildsystems/vcpkg.cmake
```

### 4. Build and Run the example project with CLion

## Instructions for Visual Studio

### 1. Install [Vcpkg](https://github.com/microsoft/vcpkg)

```cmd
git clone https://github.com/microsoft/vcpkg
.\vcpkg\bootstrap-vcpkg.bat
```

In order to use vcpkg with Visual Studio,
run the following command (may require administrator elevation):

```cmd
.\vcpkg\vcpkg integrate install
```

After this, you can now create a New non-CMake Project (or open an existing one).
All installed libraries are immediately ready to be `#include`'d and used
in your project without additional configuration.

### 2. Build and Run the *example.sln* with Visual Studio

> **Note**
>
> Make sure that on the project's property pages `Use Vcpkg Manifest` is turned on and `C++ Language Standard` is C++17.
> 
> ![Visual Studio Vcpkg Manifest](https://raw.githubusercontent.com/ConfigCat/cpp-sdk/master/media/vs-vcpkg-manifest.png "Visual Studio Vcpkg Manifest")
>
> ![Visual Studio C++17](https://raw.githubusercontent.com/ConfigCat/cpp-sdk/master/media/vs-cpp17.png "Visual Studio C++17")

## Documentation
- [ConfigCat](https://configcat.com)
