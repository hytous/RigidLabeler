# RigidLabeler Windows 安装包制作指南

本文档详细说明如何将 RigidLabeler 打包为 Windows 安装程序。

## 前置要求

### 1. 开发环境
- **Qt 6.7.3 MinGW 64-bit** (或更高版本)
- **Python 3.10+** (推荐 3.11)
- **PyInstaller** (`pip install pyinstaller`)
- **Inno Setup 6** ([下载地址](https://jrsoftware.org/isdl.php))

### 2. 确认 Qt 环境变量
确保 Qt 的 bin 目录在 PATH 中，或者在运行脚本时指定 QtPath 参数。

## 打包步骤

### 步骤 1: 构建 Release 版本的前端

1. 打开 Qt Creator
2. 打开 `frontend/frontend.pro`
3. 选择 **Release** 配置（不是 Debug）
4. 点击 **构建** → **构建项目**
5. 确认生成了 `frontend.exe`

### 步骤 2: 编译翻译文件

在 Qt Creator 中：
1. **工具** → **外部** → **Qt语言家** → **发布翻译(lrelease)**

或者命令行：
```powershell
cd frontend/translations
lrelease rigidlabeler_zh.ts
```

### 步骤 3: 安装 Python 依赖

```powershell
cd backend
pip install -r requirements.txt
pip install pyinstaller
```

### 步骤 4: 运行打包脚本

```powershell
cd packaging
.\build_installer.ps1 -QtPath "C:\Qt\6.7.3\mingw_64"
```

#### 脚本参数说明

| 参数 | 说明 | 默认值 |
|------|------|--------|
| `-QtPath` | Qt 安装路径 | `C:\Qt\6.7.3\mingw_64` |
| `-BuildType` | 构建类型 | `Release` |
| `-SkipFrontend` | 跳过前端打包 | False |
| `-SkipBackend` | 跳过后端打包 | False |
| `-SkipInstaller` | 跳过安装程序生成 | False |

### 步骤 5: 获取安装包

成功后，安装程序位于：
```
packaging/Output/RigidLabeler_Setup_1.0.1.exe
```

## 目录结构

打包后的文件结构：
```
dist/
├── frontend/               # Qt 前端
│   ├── frontend.exe
│   ├── Qt6Core.dll
│   ├── Qt6Gui.dll
│   ├── Qt6Widgets.dll
│   ├── Qt6Network.dll
│   ├── platforms/
│   ├── styles/
│   └── translations/
│       └── rigidlabeler_zh.qm
├── backend/                # Python 后端
│   └── rigidlabeler_backend/
│       ├── rigidlabeler_backend.exe
│       └── [依赖文件...]
├── config/                 # 配置文件
│   └── config.yaml
└── RigidLabeler.bat        # 启动脚本
```

## 安装包功能

生成的安装程序支持：

- ✅ 选择安装路径
- ✅ 创建开始菜单快捷方式
- ✅ 创建桌面快捷方式（可选）
- ✅ 支持多语言（英文/中文）
- ✅ 完整卸载功能
- ✅ 安装后自动启动

## 常见问题

### Q: windeployqt 找不到？
A: 确保指定正确的 Qt 路径，例如：
```powershell
.\build_installer.ps1 -QtPath "D:\Qt\6.7.3\mingw_64"
```

### Q: PyInstaller 打包失败？
A: 检查是否安装了所有依赖：
```powershell
pip install fastapi uvicorn pydantic pyyaml numpy opencv-python Pillow torch
```

### Q: 安装后程序无法启动？
A: 可能原因：
1. 后端服务启动失败 - 检查端口 8000 是否被占用
2. 缺少 VC++ 运行时 - 安装 [VC++ Redistributable](https://aka.ms/vs/17/release/vc_redist.x64.exe)

### Q: 如何更改版本号？
A: 编辑 `packaging/installer.iss` 中的 `MyAppVersion` 定义。

## 手动打包步骤

如果自动脚本失败，可以手动执行：

### 1. 部署 Qt 前端
```powershell
# 复制 exe
mkdir dist\frontend
copy frontend\build\...\release\frontend.exe dist\frontend\

# 运行 windeployqt
C:\Qt\6.7.3\mingw_64\bin\windeployqt.exe --release dist\frontend\frontend.exe
```

### 2. 打包 Python 后端
```powershell
cd backend
pyinstaller --name rigidlabeler_backend --onedir --noconsole ^
    --hidden-import uvicorn.logging ^
    --hidden-import uvicorn.loops.auto ^
    --hidden-import uvicorn.protocols.http.auto ^
    scripts\run_server.py
```

### 3. 编译安装程序
```powershell
"C:\Program Files (x86)\Inno Setup 6\ISCC.exe" packaging\installer.iss
```

## 测试安装包

1. 在干净的 Windows 系统（或虚拟机）上测试
2. 确认不需要预装任何依赖
3. 测试所有功能是否正常
4. 测试卸载是否干净

## 签名（可选）

为了避免 Windows SmartScreen 警告，建议对安装包进行数字签名：
```powershell
signtool sign /f certificate.pfx /p password /t http://timestamp.digicert.com RigidLabeler_Setup.exe
```
