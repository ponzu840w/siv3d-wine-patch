# Siv3D Wine Patch
A hack to run old siv3d windows apps under wine

A lightweight launcher and DLL injection tool designed to fix Windows applications that crash due to improper COM/OLE initialization when calling `RegisterDragDrop()`.

This tool was originally developed to fix an issue with applications built using [Siv3D (https://siv3d.github.io/en-us/)](https://siv3d.github.io/en-us/) v0.6.14 and earlier (which ran fine on Windows but broke under Wine due to tricky OLE initialization; see the `wine_incompatibility_demo` directory for details), but can be used with any Windows application that exhibits similar behavior.

> [!NOTE]
> The Windows version of Siv3D relies on DirectX 11, so a graphics API translation layer such as DXVK or D3DMetal is required to run it under Wine. This patch does not provide that, so you must prepare it separately.

# The Problem
Before you can call an API that uses OLE, such as `RegisterDragDrop()`, you must initialize COM/OLE with `OleInitialize()`. If the initialization is not performed properly for some reason, it may cause problems in your application.

# The Solution
This project provides a two-part solution:

- The Launcher (`launcherXX.exe`):
  - Starts the target application in a suspended state.
  - Injects a patch DLL (`patchXX.dll`) into the target process using `CreateRemoteThread()`.
  - Resumes the target application.
- The Patch (`patchXX.dll`):
  - Uses [MinHook](https://github.com/TsudaKageyu/minhook) to intercept calls to `RegisterDragDrop()`.
  - If uninitialized, it calls `OleInitialize(NULL)` just before the original function executes.

# Usage
## Setup
Copy the appropriate launcher and DLL to the same directory as your target application.
- For 64-bit applications: Use `launcher64.exe` and `patch64.dll`.
- For 32-bit applications: Use `launcher32.exe` and `patch32.dll`.

## Run
Auto-Detection
```
./launcher64.exe
```
Manual
```
./launcher64.exe TargetApp.exe
```
Custom DLL
```
./launcher64.exe --dll "libs/my_patch.dll" TargetApp.exe
```

# Build
```
make
```

## Prerequisites
- MinGW-w64
  - `brew install mingw-w64`
- make


# License
MinHook is licensed under the BSD 2-Clause License (Copyright (c) 2009-2017 Tsuda Kageyu).
