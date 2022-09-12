# ConfigCat C++ Console Sample App

This is a simple application to demonstrate how to use the ConfigCat C++ SDK in a console application.

## CMake

### Install [CMake](https://cmake.org/)

- Follow the instructions on https://cmake.org/install 

### Install [vcpkg](https://github.com/microsoft/vcpkg)

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

### Build with CMake

```bash
  cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=[path to vcpkg]/scripts/buildsystems/vcpkg.cmake
  cmake --build build
```

### Run
- On Windows:
  ```cmd
  .\build\example.exe
  ```

- On Linux/Mac:
  ```bash
  ./build/example
  ```

## CLion

## Visual Studio

## Documentation
- [ConfigCat](https://configcat.com)
