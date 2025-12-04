# RigidLabeler

A lightweight desktop tool for **2D geometric transformation labeling** (Rigid / Similarity / Affine) built with **Qt C++ frontend** and **FastAPI Python backend**.

> 简单说：这是一个用来给两张 2D 图像"打配准变换矩阵标签"的小工具，方便后续深度学习训练和算法评测。

---

## ✨ Features

- 🎯 **2D 几何变换标注**
  - 支持通过人工点选对应点，拟合出两张图像之间的 2D 变换
  - **三种变换模式**：
    - **刚性 (Rigid)**：旋转 + 平移（最少 2 个点）
    - **相似 (Similarity)**：旋转 + 平移 + 均匀缩放（最少 2 个点）
    - **仿射 (Affine)**：旋转 + 平移 + 非均匀缩放 + 剪切（最少 3 个点）
  - 输出旋转角度 `θ`、平移 `(tx, ty)`、缩放 `(scale_x, scale_y)`、剪切 `shear`，以及 3×3 齐次变换矩阵

- 🖱️ **交互式点对标注**
  - 左右双视图显示 Fixed / Moving 图像
  - 鼠标点击添加 / 删除 / 编辑对应点（tie points）
  - 支持在表格中精细调整坐标
  - **点击选择**：点击图像中的标记点即可选中
  - **框选多点**：Shift + 拖动创建选择框，批量选择点
  - **Ctrl+Z 撤销**：支持撤销添加/删除点操作
  - **导入/导出点对**：支持 CSV 格式导入导出点对数据

- 📐 **灵活的坐标系统**
  - 支持两种坐标原点模式：**图像中心**（默认）或 **左上角**
  - 以图像中心为原点更符合刚性变换的数学表达
  - 可切换到左上角原点模式以兼容传统像素坐标

- ⏱️ **实时计算模式**
  - 当有 3 个以上完整点对时可启用
  - 添加完整点对后 10 秒自动触发变换计算
  - 方便快速迭代标注和验证

- 🔍 **灵活的视图操控**
  - 滚轮缩放（可同步两侧视图）
  - Ctrl + 拖动进行平移
  - 双视图缩放/平移同步选项

- 📐 **稳健的变换估计**
  - 使用质心对齐 + SVD（Procrustes 分析）估计几何变换
  - 支持 **仿射变换分解**：通过 SVD 分解提取旋转、缩放和剪切分量

- 💾 **统一标签格式**
  - 标签使用 JSON 格式保存
  - 包含：图像路径、变换参数、3×3 矩阵、点对列表
  - 方便直接接入 Python / PyTorch 训练代码

- 📁 **批量标注工作流**
  - Fixed / Moving 图像独立目录记忆
  - Next Fixed / Next Moving / Next Pair 快速切换图像
  - **GT 文件夹导出**：一键导出 3×3 矩阵到 GT 文件夹（编号 txt 文件）

- 🧱 **清晰的前后端分层**
  - 前端：Qt C++，负责 UI、交互和状态管理
  - 后端：FastAPI + Python，负责变换计算与标签读写
  - 前后端通过 HTTP/JSON 通信，协议简单易调试

---

## 🏗 Architecture

整体采用 **本地桌面应用 + 本地 HTTP 服务** 的方式运行：

```
+------------------+          HTTP (JSON)           +------------------------+
|  Qt Frontend     |  <-------------------------->  |  FastAPI Backend       |
|  - MainWindow    |                                |  - /health             |
|  - ImageView     |                                |  - /compute/rigid      |
|  - TiePointTable |                                |  - /labels/save        |
+------------------+                                |  - /labels/load        |
                                                    +------------------------+
```

- Qt 前端负责：
  - 加载 / 显示图像
  - 允许用户点选、管理点对
  - 请求后端计算刚性变换并显示结果
  - 触发标签保存 / 加载
- FastAPI 后端负责：
  - 用数学方法从点对中估计刚性变换
  - 将变换参数封装成统一标签格式
  - 将标签保存为 JSON 文件

------

## 📂 Project Structure

```text
RigidLabeler/
├── README.md
├── start_backend.bat          # Windows 启动后端脚本
├── start_backend.ps1          # PowerShell 启动后端脚本
│
├── config/
│   ├── app.yaml               # 前端配置（后端URL、默认路径等）
│   └── backend.yaml           # 后端配置（端口、标签路径等）
│
├── docs/
│   ├── design_overview.md     # 架构说明
│   ├── api_spec.md            # 前后端接口文档
│   ├── config_spec.md         # 配置文件规范
│   └── label_format.md        # 标签格式规范
│
├── data/
│   ├── images/                # 示例图像目录
│   ├── labels/                # 标签输出目录
│   └── temp/                  # 临时文件目录
│
├── frontend/                  # Qt C++ 客户端
│   ├── frontend.pro           # Qt 项目文件
│   ├── main.cpp               # 程序入口
│   ├── mainwindow.h           # 主窗口头文件
│   ├── mainwindow.cpp         # 主窗口实现
│   ├── mainwindow.ui          # Qt Designer UI 文件
│   │
│   ├── app/                   # 应用层
│   │   ├── AppConfig.h/cpp    # 配置管理
│   │   └── BackendClient.h/cpp # HTTP 客户端
│   │
│   └── model/                 # 数据模型
│       ├── ImagePairModel.h/cpp   # 图像对模型
│       └── TiePointModel.h/cpp    # 点对数据模型
│
└── backend/                   # FastAPI Python 后端
    ├── requirements.txt       # Python 依赖
    │
    ├── scripts/
    │   └── run_server.py      # 启动后端服务
    │
    └── rigidlabeler_backend/
        ├── __init__.py
        ├── config.py          # 读取 backend.yaml
        │
        ├── api/               # FastAPI 接口定义
        │   ├── __init__.py
        │   ├── server.py      # FastAPI 应用入口
        │   └── schemas.py     # Pydantic 请求/响应模型
        │
        ├── core/              # 核心算法逻辑
        │   ├── __init__.py
        │   └── transforms.py  # 2D 刚性变换估计 (SVD)
        │
        ├── io/                # 数据/标签存取
        │   ├── __init__.py
        │   ├── image_loader.py    # 图像读写（OpenCV）
        │   └── label_store.py     # JSON 标签读写
        │
        ├── utils/
        │   ├── __init__.py
        │   ├── logging_utils.py
        │   └── path_utils.py
        │
        └── tests/             # 单元测试
            ├── __init__.py
            ├── test_transforms.py
            ├── test_label_store.py
            └── test_api.py
```

------

## 🚀 Quick Start

### 1. 启动后端服务

```bash
# 安装 Python 依赖
cd backend
pip install -r requirements.txt

# 启动后端（默认端口 8000）
python scripts/run_server.py

# 或使用启动脚本
# Windows: start_backend.bat
# PowerShell: .\start_backend.ps1
```

### 2. 构建并运行前端

```bash
# 使用 Qt Creator 打开 frontend/frontend.pro
# 或使用 qmake 命令行构建

cd frontend
qmake frontend.pro
make  # Linux/macOS
# 或 mingw32-make / nmake (Windows)

./frontend  # 运行程序
```

### 3. 使用流程

1. 点击 **Load Fixed Image** 加载基准图像
2. 点击 **Load Moving Image** 加载待配准图像
3. 点击 **Add Point** 进入添加点模式
4. 在两张图像上依次点击对应点（先 Fixed，后 Moving）
5. 重复步骤 3-4 添加至少 2 个点对
6. 点击 **Compute Transform** 计算变换参数
7. 点击 **Save Label** 保存标签

### 4. 快捷操作

- **视图操控**
  - 滚轮：缩放图像
  - Ctrl + 拖动：平移图像
  - Shift + 拖动：框选多个点
- **快捷键**
  - Ctrl+Z：撤销操作
  - Ctrl+Shift+Z：重做操作
  - Ctrl+1：加载 Fixed 图像
  - Ctrl+2：加载 Moving 图像
  - Ctrl+R：计算变换
  - Ctrl+S：保存标签
- **批量标注**
  - 点击 **Next >** 按钮快速切换到同目录下一张图像
  - 点击 **Next Pair >>** 同时切换 Fixed 和 Moving 图像
  - 点击 **Export to GT Folder** 导出 3×3 矩阵到 GT 文件夹

------

## 📡 API Overview (FastAPI)

> 详细字段可以在 `docs/api_spec.md` 中查看。

### `GET /health`

健康检查接口。

### `POST /compute/rigid`

根据点对估计 2D 刚性变换。

**Request:**
```json
{
  "tie_points": [
    { "fixed": {"x": 100, "y": 200}, "moving": {"x": 110, "y": 195} },
    { "fixed": {"x": 300, "y": 150}, "moving": {"x": 315, "y": 140} }
  ],
  "allow_scale": false,
  "min_points_required": 2
}
```

**Response:**
```json
{
  "success": true,
  "rigid": {
    "theta_deg": 5.3,
    "tx": 12.4,
    "ty": -3.1,
    "scale": 1.0
  },
  "matrix_3x3": [[...], [...], [...]],
  "rms_error": 0.82,
  "num_points": 2
}
```

### `POST /labels/save`

保存一对图像的标签。

### `POST /labels/load`

加载某一对图像的已有标签。

------

## 🧾 Label Format

统一的标签结构（JSON）：

```json
{
  "version": "1.0",
  "image_fixed": "data/images/vis_001.png",
  "image_moving": "data/images/ir_001.png",
  "rigid": {
    "theta_deg": 5.3,
    "tx": 12.4,
    "ty": -3.1,
    "scale": 1.0
  },
  "matrix_3x3": [
    [0.995, -0.099, 12.4],
    [0.099,  0.995, -3.1],
    [0.0,    0.0,   1.0]
  ],
  "tie_points": [
    { "fixed": {"x": 120.5, "y": 80.2}, "moving": {"x": 130.1, "y": 75.9} },
    { "fixed": {"x": 200.0, "y": 150.0}, "moving": {"x": 210.3, "y": 143.2} }
  ],
  "timestamp": "2025-11-30T12:00:00",
  "comment": ""
}
```

详细格式说明请参考 `docs/label_format.md`。

------

## 📌 Roadmap

- [x] 基本 UI：加载图像、双视图显示、点对管理
- [x] 基本 2D 刚性变换估计（R + t [+ s]）
- [x] 标签导出：JSON 格式
- [x] 后端 API：健康检查、计算变换、保存/加载标签
- [ ] 预览功能：显示 Warp 后的重叠效果
- [ ] 工程管理：批量管理多对图像的标签
- [ ] 键盘快捷键 / 高级编辑功能
- [ ] 跨平台打包（Windows / Linux / macOS）

------

## 📄 License

MIT License
