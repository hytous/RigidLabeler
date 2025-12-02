# RigidLabeler 开发维护日志

本文档按时间倒序记录开发过程中的需求变更、问题修复和实现方式，便于学习和回溯。

---

## #012 - 2025-12-02

### 问题
选中标记后只有 tiePointsTable 中会颜色加深，而没有在图像中显示选中圆圈。

### 原因
`setupConnections()` 在 `setModel()` 之前被调用。连接 `selectionChanged` 信号时，`ui->tiePointsTable->selectionModel()` 还是旧的或空的。当 `setModel()` 被调用时会创建新的 selectionModel，导致之前的信号连接失效。

### 修复
将 `ui->tiePointsTable->setModel(m_tiePointModel)` 移到 `setupConnections()` 之前调用。

---

## #011 - 2025-12-02

### 需求
选中标记的突出显示不够明显，需要更醒目的视觉反馈。

### 实现
在 `createCrosshairMarker()` 函数中，当 `highlighted=true` 时，额外绘制一个反色虚线圆圈（半径18像素）包围十字标记。

### 技术细节
- 使用 `QGraphicsEllipseItem` 绘制圆圈
- 使用 `Qt::DashLine` 样式的 `QPen` 绘制虚线
- 圆圈颜色使用与十字标记边框相同的反色（亮色背景用黑色，暗色背景用白色）

---

## #010 - 2025-12-02

### 需求
单击图像空白处时取消选中。

### 实现
在 `eventFilter()` 的普通点击处理中，当 `findPointAtPosition()` 返回 -1（未点击到任何标记）时，调用 `ui->tiePointsTable->clearSelection()` 清除选择。

---

## #009 - 2025-12-02

### 需求
1. Ctrl+S 自动导出点对（类似导出矩阵的逻辑）
2. 十字标记添加反色边缘使其在任何背景上都清晰可见
3. Shift+拖动批量选中、Delete 删除、Ctrl+Z 撤销

### 实现

#### Ctrl+S 快速导出点对
- 新增 `quickExportTiePoints()` 函数
- 新增成员变量 `m_tiePointsExportDir` 记住导出目录
- 在 `keyPressEvent()` 中捕获 Ctrl+S 快捷键
- 首次按下时弹出文件夹选择对话框，之后自动保存
- 文件名与固定图像相同，格式为 CSV

#### 十字标记反色边缘
- 修改 `createCrosshairMarker()` 函数
- 根据标记颜色亮度决定边框颜色（亮色用黑边，暗色用白边）
- 先绘制较粗的边框线条，再绘制主色线条
- 中心圆点也添加边框

#### 批量选中与删除
- 框选功能已存在（`handleRubberBandSelection`）
- 添加 Delete 键快捷键到 `keyPressEvent()`
- 删除使用 `QUndoStack::beginMacro()` 支持批量撤销

---

## #008 - 2025-12-02

### 问题
选择配准模式时，鼠标悬停在选项上显示的介绍没有汉化。

### 原因
ComboBox 的 `setItemData(..., Qt::ToolTipRole)` 是在构造函数中设置的，虽然使用了 `tr()`，但语言切换时 `retranslateUi()` 不会更新这些动态设置的数据。

### 修复
在 `switchToEnglish()` 和 `switchToChinese()` 函数中，重新调用 `setItemData()` 设置翻译后的 tooltip。

---

## #007 - 2025-12-02

### 需求
更新版本号从 0.1.0 到 0.1.1

### 实现
更新以下文件中的版本号：
- `backend/rigidlabeler_backend/__init__.py`
- `docs/api_spec.md`
- `docs/design_overview.md`
- `docs/label_format.md`
- `frontend/mainwindow.cpp` (About 对话框)
- `frontend/translations/rigidlabeler_zh.ts`
- `packaging/installer.iss`

---

## #006 - 2025-12-01

### 需求
将均匀缩放（scale）改为独立的 scale_x、scale_y，支持仿射变换。

### 实现

#### 后端
- `transforms.py`: 新增 `compute_affine_transform()` 和 `compute_similarity_transform()`
- `schemas.py`: `RigidParams` 添加 `scale_x`, `scale_y`, `shear` 字段
- `server.py`: `/compute/rigid` 接口添加 `transform_mode` 参数

#### 前端
- `BackendClient.h/cpp`: 更新 `RigidParams` 结构体
- `mainwindow.ui`: 移除 `chkAllowScale` 复选框，添加 `cmbTransformMode` 下拉框
- `mainwindow.cpp`: 添加三种模式的 tooltip，动态调整最小点数要求

#### 变换模式
| 模式 | 参数 | 最少点数 |
|------|------|---------|
| Rigid | θ, tx, ty | 2 |
| Similarity | θ, tx, ty, scale | 2 |
| Affine | θ, tx, ty, scale_x, scale_y, shear | 3 |

---

## #005 - 2025-12-01

### 需求
创建 Windows 安装程序打包脚本

### 实现
- `packaging/build_installer.ps1`: PowerShell 构建脚本
- `packaging/installer.iss`: Inno Setup 安装脚本
- `launcher/main.cpp`: 纯 Win32 API 启动器（无 Qt 依赖）

#### 打包流程
1. 编译 C++ 启动器
2. 使用 PyInstaller 打包 Python 后端
3. 使用 windeployqt 收集 Qt 依赖
4. 使用 Inno Setup 生成安装程序

---

## #004 - 2025-12-01

### 问题
后端依赖 OpenCV (cv2) 导致打包体积过大

### 修复
将 `image_loader.py` 中的 OpenCV 替换为 PIL (Pillow)：
- `cv2.imread()` → `Image.open()`
- `cv2.resize()` → `Image.resize()`

---

## #003 - 2025-12-01

### 需求
移除未使用的叠加预览（overlay）功能

### 实现
- 移除 `mainwindow.ui` 中的相关控件
- 移除 `mainwindow.cpp` 中的相关代码
- 保留棋盘格预览功能

---

## #002 - 2025-11-30

### 需求
分离"计算变换"和"自动保存"按钮

### 实现
原来的 Fit 按钮同时执行计算和保存，现在分为：
- `btnCompute`: 仅计算变换
- `btnSaveLabel`: 保存标签

---

## #001 - 2025-11-30

### 项目初始化

RigidLabeler 是一个用于 2D 刚性变换标注的桌面工具。

#### 技术架构
- **前端**: Qt 6.7.3 C++ (MinGW 64-bit)
- **后端**: Python FastAPI + uvicorn
- **通信**: HTTP/JSON (localhost:8000)

#### 核心功能
- 加载固定/移动图像对
- 交互式标注对应点（tie points）
- 计算刚性/相似变换
- 保存/加载 JSON 格式标签
