# LpcAnalyzer
Low Pin Count (LPC) Analyzer for Saleae Logic

## build
clone the repo with submodules

### msvc / vscode
using "cmake tools" extension, configure for Release build with VS - amd64 and build.

### msvc / cmdline
```
"c:\Program Files (x86)\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" amd64
```
(or equivalent for your installation / start environment from appropriate start menu entry)

```
mkdir build && cd build
cmake .. -GNinja && ninja
```

## usage
add the path containing the dll to Logic (Preferences -> Custom Low Level Analyzers), or copy dll to existing setup path.
