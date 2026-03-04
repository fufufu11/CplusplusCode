# 9. 构建系统与 CMake

## 9.1 为什么选择 CMake + G++？
本项目采用 **CMake** 作为构建系统，并推荐使用 **G++ (GCC)** 作为编译器。
- **跨平台一致性**：G++ 是 Linux 环境下的标准编译器。在 Windows 上使用 G++ (MinGW-w64) 可以最大限度地模拟 Linux 的编译行为，减少因编译器差异导致的代码移植问题。
- **构建标准化**：CMake 是 C++ 界的通用标准，能够自动处理依赖管理、编译选项配置和跨平台构建脚本生成。

## 9.2 本项目的构建工具链
为了方便在 Windows 上进行类 Linux 开发，本项目配置了以下工具链：
- **编译器**：G++ (MinGW-w64 GCC 15.2.0+)
- **构建生成器**：Ninja (推荐) 或 MinGW Makefiles
- **构建脚本**：`build.ps1` (Windows PowerShell)

该脚本会自动检测系统中的 `ninja` 和 `g++`，优先构建 MinGW 环境。

## 9.3 如何搭建开发环境 (Windows)
为了获得最佳体验，建议安装 **MSYS2** 或直接下载 **MinGW-w64**。

**推荐方案 (MSYS2)**：
1. 下载并安装 [MSYS2](https://www.msys2.org/)。
2. 打开 MSYS2 UCRT64 或 MINGW64 终端，执行以下命令安装工具链：
   ```bash
   pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja
   ```
3. 将 `C:\msys64\mingw64\bin` (默认路径) 添加到系统环境变量 PATH 中。

## 9.4 使用 build.ps1 构建
在项目根目录下运行：
```powershell
.\build.ps1
```
脚本会自动寻找 G++ 和 Ninja/Make 进行编译。

**构建产物路径**：
- **可执行文件**：`build/bin/` (例如 `build/bin/DistributedKV_bin.exe`)
- **静态库/存档**：`build/lib/`

若需清理并重新构建：
```powershell
.\build.ps1 --clean
```

## 10.5 运行与验证
构建完成后，你可以通过以下命令运行程序：

**1. 运行 Demo 主程序**：
```powershell
.\build\bin\DistributedKV_bin.exe
```

**2. 运行单元测试 (通过 CTest)**：
```powershell
ctest --test-dir build --output-on-failure
```

**3. 直接运行测试程序**：
```powershell
.\build\bin\skiplist_test.exe
```

## 10.6 VS Code 快捷键与集成
项目已针对 VS Code 深度优化，推荐使用以下快捷方式：

- **一键运行/调试**：按下 `F5`（会自动触发编译并运行主程序）。
- **运行单元测试**：在左侧"测试"面板点击运行，或在调试面板选择 `DistributedKV: Run SkipList Test`。
- **清理并重新配置**：`Ctrl+Shift+P` -> 输入 `CMake: Delete Cache and Reconfigure`。
