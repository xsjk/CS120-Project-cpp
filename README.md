# CS120

## Setup
```shell
git clone git@github.com:xsjk/CS120-Project-cpp.git
cd CS120-Project-cpp
git checkout ASIO
cmake -B build -G "MinGW Makefiles"
```

## Build
Recommended to use cmake plugin in vscode to build, or manually build with cmake
```shell
cmake --build build --config Release -j 8
```
