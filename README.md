# RigidLabeler

A lightweight desktop tool for **2D rigid transformation labeling** (rotation + translation [+ optional uniform scale]) built with **Qt C++ frontend** and **FastAPI Python backend**.

> 简单说：这是一个用来给两张 2D 图像“打刚性配准矩阵标签”的小工具，方便后续深度学习训练和算法评测。

---

## ✨ Features

- 🎯 **2D 刚性变换标注**
  - 支持通过人工点选对应点，拟合出两张图像之间的 2D 刚性变换
  - 输出旋转角度 `θ`、平移 `(tx, ty)`，以及 3×3 齐次变换矩阵

- 🖱️ **交互式点对标注**
  - 左右双视图显示 Base / Warp 图像
  - 鼠标点击添加 / 删除 / 编辑对应点（tie points）
  - 支持在表格中精细调整坐标

- 📐 **稳健的变换估计**
  - 使用质心对齐 + SVD（Procrustes 分析）估计刚性变换
  - 支持可选的 **uniform scale**（旋转 + 等比缩放 + 平移）

- 💾 **统一标签格式**
  - 标签使用 JSON/CSV 等格式保存
  - 包含：图像路径、变换参数、3×3 矩阵、点对列表
  - 方便直接接入 Python / PyTorch 训练代码

- 🧱 **清晰的前后端分层**
  - 前端：Qt C++，负责 UI、交互和状态管理
  - 后端：FastAPI + Python，负责变换计算与标签读写
  - 前后端通过 HTTP/JSON 通信，协议简单易调试

---

## 🏗 Architecture

整体采用 **本地桌面应用 + 本地 HTTP 服务** 的方式运行：

+------------------+          HTTP (JSON)           +------------------------+
|  Qt Frontend     |  <-------------------------->  |  FastAPI Backend       |
|  - MainWindow    |                                |  - /compute_rigid      |
|  - ImageView     |                                |  - /labels/save        |
|  - TiePointTable |                                |  - /labels/load        |
+------------------+                                +------------------------+

- Qt 前端负责：
  - 加载 / 显示图像
  - 允许用户点选、管理点对
  - 请求后端计算刚性变换并显示结果
  - 触发标签保存 / 加载
- FastAPI 后端负责：
  - 用数学方法从点对中估计刚性变换
  - 将变换参数封装成统一标签格式
  - 将标签保存为 JSON/CSV/NPY 等文件

------

## 📂 Project Structure

> 目录结构是为 Qt + FastAPI + Python 工程设计的参考实现，你可以根据需要微调。

```text
RigidLabeler/
├─ README.md
├─ LICENSE
├─ .gitignore
├─ CMakeLists.txt              # 顶层 CMake（驱动 Qt 子工程）
│
├─ config/
│   ├─ app.yaml                # 前端配置（默认路径、语言等）
│   └─ backend.yaml            # 后端地址、端口等
│
├─ docs/
│   ├─ design_overview.md      # 架构说明
│   └─ api_spec.md             # 前后端接口文档
│
├─ data/
│   ├─ images/                 # 示例图像
│   ├─ labels/                 # 标签输出目录
│   └─ projects/               # （可选）工程文件
│
├─ logs/
│   ├─ frontend/
│   └─ backend/
│
├─ frontend/                   # Qt C++ 客户端
│   ├─ CMakeLists.txt
│   ├─ include/
│   │   ├─ app/
│   │   │   ├─ MainWindow.h
│   │   │   ├─ AppConfig.h
│   │   │   └─ BackendClient.h     # 封装 HTTP 请求
│   │   ├─ ui/
│   │   │   ├─ ImageViewWidget.h   # 显示单张图像的控件
│   │   │   ├─ TiePointTableWidget.h
│   │   │   └─ ToolbarWidget.h
│   │   └─ model/
│   │       ├─ ImagePairModel.h    # 管理一对图像及其状态
│   │       └─ TiePointModel.h     # 点对数据模型
│   │
│   ├─ src/
│   │   ├─ main.cpp
│   │   ├─ app/
│   │   │   ├─ MainWindow.cpp
│   │   │   ├─ AppConfig.cpp
│   │   │   └─ BackendClient.cpp
│   │   ├─ ui/
│   │   │   ├─ ImageViewWidget.cpp
│   │   │   ├─ TiePointTableWidget.cpp
│   │   │   └─ ToolbarWidget.cpp
│   │   └─ model/
│   │       ├─ ImagePairModel.cpp
│   │       └─ TiePointModel.cpp
│   │
│   ├─ resources/
│   │   ├─ icons/
│   │   ├─ styles/                 # qss 皮肤
│   │   └─ qml/                    # 若使用 QML
│   └─ tests/
│
└─ backend/
    ├─ rigidlabeler_backend/
    │   ├─ __init__.py
    │   ├─ config.py               # 读取 backend.yaml
    │   │
    │   ├─ api/                    # FastAPI 接口定义
    │   │   ├─ __init__.py
    │   │   ├─ server.py           # FastAPI 应用入口
    │   │   └─ schemas.py          # Pydantic 请求/响应模型
    │   │
    │   ├─ core/                   # 核心算法逻辑
    │   │   ├─ transforms.py       # 2D 刚性变换估计 (R, t, [s])
    │   │   ├─ tie_points.py       # 点对校验/预处理
    │   │   └─ label_ops.py        # 标签对象构建与转换
    │   │
    │   ├─ io/                     # 数据/标签存取
    │   │   ├─ image_loader.py     # 图像读写（OpenCV/PIL）
    │   │   ├─ label_store.py      # JSON/CSV/NPY 标签读写
    │   │   └─ project_store.py    # （可选）工程文件读写
    │   │
    │   ├─ utils/
    │   │   ├─ logging_utils.py
    │   │   └─ path_utils.py
    │   │
    │   └─ tests/
    │       ├─ test_transforms.py
    │       ├─ test_label_store.py
    │       └─ test_api.py
    │
    └─ scripts/
        ├─ run_server.py           # 一键启动后端
        ├─ export_labels.py        # 标签格式转换脚本
        └─ demo_generate.py        # demo 数据生成与自测
```

------

## 📡 API Overview (FastAPI)

> 这里只列出核心接口，详细字段可以在 `docs/api_spec.md` 中查看。

### `POST /compute_rigid`

根据点对估计 2D 刚性变换。

**Request (示例):**

```json
{
  "points": [
    { "base": [x1, y1], "warp": [x1p, y1p] },
    { "base": [x2, y2], "warp": [x2p, y2p] }
  ],
  "allow_scale": false
}
```

**Response (示例):**

```json
{
  "theta_deg": 5.3,
  "tx": 12.4,
  "ty": -3.1,
  "scale": 1.0,
  "matrix_3x3": [
    [0.995, -0.099, 12.4],
    [0.099,  0.995, -3.1],
    [0.0,    0.0,   1.0]
  ],
  "rms_error": 0.82
}
```

### `POST /labels/save`

保存一对图像的标签（含多种信息：变换、点对等）。

### `GET /labels/load`

加载某一对图像的已有标签。

------

## 🧾 Label Format (Example)

统一的标签结构（JSON 示例）：

```json
{
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
    { "base": [120.5, 80.2], "warp": [130.1, 75.9] },
    { "base": [200.0, 150.0], "warp": [210.3, 143.2] }
  ]
}
```

你可以在 `backend/io/label_store.py` 中自由扩展字段，以适配你的数据管线和训练代码。

------

## 📌 Roadmap (Work in Progress)

-  基本 UI：加载图像、双视图显示、点对管理
-  基本 2D 刚性变换估计（R + t [+ s]）
-  标签导出：JSON / CSV / NPY
-  预览功能：显示 Warp 后的重叠效果
-  工程管理：批量管理多对图像的标签
-  键盘快捷键 / 高级编辑功能
-  跨平台打包（Windows / Linux）
