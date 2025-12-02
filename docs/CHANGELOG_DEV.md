# RigidLabeler 开发维护日志

本文档按时间倒序记录开发过程中的需求变更、问题修复和实现方式，便于学习和回溯。

---

## #022 - 2025-12-02

### 需求
保存并恢复语言设置（中文/英文）。

### 实现

#### AppConfig 扩展
- `optionLanguage()` / `setOptionLanguage()` - 存取语言代码 ("zh" / "en")

#### 启动时恢复
在构造函数中根据保存的语言设置加载翻译文件：
```cpp
QString savedLang = AppConfig::instance().optionLanguage();
if (savedLang == "en") {
    m_currentLanguage = "en";
    ui->actionLangEnglish->setChecked(true);
} else {
    m_currentLanguage = "zh";
    ui->actionLangChinese->setChecked(true);
    // Load Chinese translation
    m_translator->load("rigidlabeler_zh", ...);
    qApp->installTranslator(m_translator);
}
```

#### 切换时保存
修改语言切换 Action 的信号连接，切换后自动保存语言设置。

---

## #021 - 2025-12-02

### 需求
保存并恢复 Options 区域的选项状态，包括：
- 坐标原点模式（Top-Left / Center）
- 显示点标签
- 同步缩放
- 变换模式（Rigid / Similarity / Affine）

### 实现

#### AppConfig 扩展
添加 UI 选项的存取方法：
- `optionOriginTopLeft()` / `setOptionOriginTopLeft()`
- `optionShowPointLabels()` / `setOptionShowPointLabels()`
- `optionSyncZoom()` / `setOptionSyncZoom()`
- `optionTransformMode()` / `setOptionTransformMode()`

#### 启动时恢复
在 `MainWindow` 构造函数中，`setupConnections()` 之前恢复选项状态：
```cpp
ui->chkOriginTopLeft->setChecked(AppConfig::instance().optionOriginTopLeft());
ui->chkShowPointLabels->setChecked(AppConfig::instance().optionShowPointLabels());
ui->chkSyncZoom->setChecked(AppConfig::instance().optionSyncZoom());
ui->cmbTransformMode->setCurrentIndex(AppConfig::instance().optionTransformMode());
```

#### 变更时保存
修改各选项的信号连接，在状态变化时自动保存到 `QSettings`。

### 技术细节
- 在 `setupConnections()` 之前设置控件状态，避免触发保存逻辑
- 同时更新成员变量 `m_useTopLeftOrigin` 和 `m_showPointLabels`

---

## #020 - 2025-12-02

### 需求
扩展工程缓存功能：
1. 保存并恢复上次的点对数据
2. 导入点对时默认使用上次导出点对的目录

### 实现

#### 点对缓存
- 在 `saveProjectState()` 中，将当前点对保存到隐藏缓存目录
- 缓存文件路径：`{固定图像目录}/.rigidlabeler_cache/{图像名}_tiepoints.csv`
- 保存格式：`fixed_x, fixed_y, moving_x, moving_y`（像素坐标）
- 支持部分点对（只有固定点或只有动态点）

#### 点对恢复
- 在 `restoreLastProject()` 中加载缓存的点对文件
- 自动调用 `updatePointDisplay()` 和 `updateActionStates()` 更新界面

#### 导入路径默认值
- 修改 `importTiePoints()` 函数
- 使用 `m_tiePointsExportDir` 作为文件对话框的默认目录
- 实现导出/导入路径的统一

---

## #019 - 2025-12-02

### 需求
实现工程缓存功能，类似 PyCharm/VSCode 的工作区记忆：
1. 记住当前正在标注的数据集路径和图像索引
2. 记住导出目录等工作状态
3. 下次打开软件自动恢复上次的工作进度

### 实现

#### AppConfig 扩展
在 `AppConfig` 类中添加工程状态保存/加载方法：
- `saveProjectState()` - 以固定图像目录为 key，保存工程状态
- `loadProjectState()` - 加载指定目录的工程状态
- `lastProjectDir()` / `setLastProjectDir()` - 记录最后打开的工程

#### 工程状态数据
每个工程保存以下信息：
- 固定图像目录和当前索引
- 动态图像目录和当前索引
- 矩阵导出目录
- 点对导出目录

#### MainWindow 生命周期
- 构造函数末尾调用 `restoreLastProject()` 恢复上次工程
- 重写 `closeEvent()` 在关闭时调用 `saveProjectState()` 保存状态

### 技术细节
- 使用 `QSettings` 存储工程缓存，按目录路径作为 key
- 目录路径中的特殊字符 (`/`, `\`, `:`) 替换为 `_` 作为 key
- 恢复时验证目录是否存在，索引是否有效
- 使用 `QTimer::singleShot(100, ...)` 延迟恢复确保 UI 就绪

---

## #018 - 2025-12-02

### 需求
在结果显示区域（txtResult）中，根据 RMS Error 数值添加颜色渐变，直观显示配准质量。

### 实现
在 `onComputeRigidCompleted()` 函数中，根据 RMS Error 值选择颜色：
- `< 1.0 px`：绿色 (#00aa00) - 优秀
- `1.0 ~ 3.0 px`：青色 (#00aaaa) - 良好
- `3.0 ~ 4.0 px`：橙色 (#ff8800) - 警告
- `>= 4.0 px`：红色 (#ff0000) - 较差

### 技术细节
- 将 `txtResult` 的内容从纯文本改为 HTML 格式
- 使用 `<span style='color:...'>` 标签为 RMS Error 行添加颜色
- 使用 `<pre>` 标签保持等宽字体排版
- 将换行符 `\n` 替换为 `<br>` 以适应 HTML

---

## #017 - 2025-12-02

### 需求
表格选中样式改为类似 Excel 的蓝色高亮。

### 实现
在 `MainWindow` 构造函数中为 `tiePointsTable` 设置 Qt 样式表：
```cpp
ui->tiePointsTable->setStyleSheet(
    "QTableView::item:selected { background-color: #0078d7; color: white; }"
    "QTableView::item:selected:focus { background-color: #0078d7; color: white; }"
);
```

---

## #016 - 2025-12-02

### 需求
当点对数量达到最小要求时，自动开启实时计算模式。

### 实现
在 `updateRealtimeComputeState()` 函数中，当 `completePairCount >= 3` 且两张图像都已加载时，自动勾选 `chkRealtimeCompute` 并启用实时计算模式。

### 代码
```cpp
// Auto-enable realtime mode when we first reach 3 points (and images are loaded)
if (canEnable && !m_realtimeComputeEnabled && m_imagePairModel->hasBothImages()) {
    ui->chkRealtimeCompute->setChecked(true);
    m_realtimeComputeEnabled = true;
    statusBar()->showMessage(tr("Real-time compute mode auto-enabled (3+ complete pairs)."), 3000);
}
```

---

## #015 - 2025-12-02

### 需求
在十字标记旁边显示点序号（可选功能）。

### 实现
1. 添加成员变量 `m_showPointLabels`，默认为 `true`
2. 在 UI 中添加 "Show Point Labels" 复选框
3. 修改 `createCrosshairMarker()` 函数签名，增加 `pointIndex` 参数
4. 当 `m_showPointLabels` 为 true 且 `pointIndex >= 0` 时，绘制文字标签
5. 标签使用主色绘制，带反色背景阴影增强可读性
6. 更新 `updatePointDisplay()` 中的调用，传入点索引

### 技术细节
- 使用 `QGraphicsSimpleTextItem` 绘制文字
- 字号 9pt，加粗
- 位置在十字标记右上角（armLength + 3, -armLength）
- 通过绘制两层文字（阴影层在下）实现边缘效果

---

## #014 - 2025-12-02

### 需求
1. 实时计算延迟从 10 秒改为 5 秒
2. 计算完成后自动刷新预览对话框（如果已打开）

### 实现

#### 缩短延迟
将 `m_realtimeComputeTimer->setInterval(10000)` 改为 `setInterval(5000)`，并更新相关状态栏提示信息。

#### 自动刷新预览
在 `onComputeRigidCompleted()` 末尾添加：
```cpp
// Auto-refresh preview dialog if it's open
if (m_previewDialog && m_previewDialog->isVisible()) {
    onPreviewRefreshRequested(m_currentPreviewGridSize);
}
```

---

## #013 - 2025-12-02

### 问题

按 Ctrl+S 有几率出现 "Please compute transform first" 的弹窗。

### 原因

快捷键冲突：
1. `mainwindow.ui` 中 `actionSaveLabel` 绑定了 `Ctrl+S`，触发 `saveLabel()` 函数（需要先计算变换）
2. `keyPressEvent()` 中也处理了 `Ctrl+S`，调用 `quickExportTiePoints()`

Qt 的事件处理顺序不确定，有时 QAction 的快捷键先触发，有时 keyPressEvent 先触发。

### 修复

移除 `mainwindow.ui` 中 `actionSaveLabel` 的 `Ctrl+S` 快捷键，让 `Ctrl+S` 专门用于快速导出点对。

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
