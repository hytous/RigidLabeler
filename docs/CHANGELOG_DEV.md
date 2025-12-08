# RigidLabeler 开发维护日志

本文档按时间倒序记录开发过程中的需求变更、问题修复和实现方式，便于学习和回溯。

---

## #031 - 2025-12-08

### 问题

切换归一化矩阵模式后，如果用户查看预览图但未手动重新计算，会使用切换前的矩阵生成预览，导致显示错误。

### 解决方案

在 `chkNormalizedMatrix` 的 toggled 信号处理中，检查当前点对数量是否满足最小要求，如满足则自动触发 `computeTransform()`。

### 实现

```cpp
connect(ui->chkNormalizedMatrix, &QCheckBox::toggled, this, [this](bool checked) {
    m_useNormalizedMatrix = checked;
    AppConfig::instance().setOptionNormalizedMatrix(checked);
    // ...状态栏消息...
    
    // Auto recompute if we have enough points to avoid preview mismatch
    int minPoints = AppConfig::instance().minPointsRequired();
    if (m_tiePointModel->completePairCount() >= minPoints) {
        computeTransform();
    }
});
```

### 修改文件

- `frontend/mainwindow.cpp`

---

## #030 - 2025-12-08

### 问题
编译错误：
1. `BackendClient.h:120: error: invalid use of incomplete type 'class QSize'`
2. `BackendClient.cpp:126: error: no matching function for call to 'QJsonArray::QJsonArray(<brace-enclosed initializer list>)'`

### 原因

1. **QSize 不完整类型**：头文件中使用了 `QSize` 作为默认参数，但未包含 `<QSize>` 头文件
2. **QJsonArray 初始化语法**：花括号初始化列表 `QJsonArray{a, b}` 在某些 Qt/编译器版本中不支持

### 解决方案

1. 在 `BackendClient.h` 中添加 `#include <QSize>`
2. 将 `QJsonArray{...}` 改为显式 `append()` 调用

### 修复代码

```cpp
// BackendClient.h - 添加头文件
#include <QSize>

// BackendClient.cpp - 修改 QJsonArray 初始化
QJsonArray fixedSizeArray;
fixedSizeArray.append(fixedImageSize.width());
fixedSizeArray.append(fixedImageSize.height());
requestBody["fixed_image_size"] = fixedSizeArray;
```

### 修改文件

- `frontend/app/BackendClient.h`
- `frontend/app/BackendClient.cpp`
- `frontend/translations/rigidlabeler_zh.ts` - 添加归一化矩阵相关中文翻译

---

## #029 - 2025-12-05

### 需求
实现归一化矩阵模式选项：用户可选择输出「像素坐标矩阵」或「归一化矩阵 [-1,1]」，后者可直接用于 `torch.affine_grid()`。

### 背景
PyTorch 的 `F.affine_grid()` 使用归一化坐标系 [-1, 1]，而原有实现输出的是像素坐标系矩阵。用户需要手动转换才能使用，不够方便。

### 设计

**归一化公式：**
```
T_norm = S_fixed @ T_pixel @ S_moving_inv
```

其中：
- `S_fixed`：将 fixed 图像像素坐标归一化的矩阵
- `T_pixel`：原始像素坐标变换矩阵（中心原点）
- `S_moving_inv`：将 moving 图像归一化坐标转回像素坐标的矩阵

```python
def normalize_matrix(matrix_3x3, fixed_size, moving_size):
    W_f, H_f = fixed_size
    W_m, H_m = moving_size
    
    # 归一化矩阵: pixel -> [-1, 1]
    S_fixed = np.array([
        [2.0 / W_f, 0, 0],
        [0, 2.0 / H_f, 0],
        [0, 0, 1]
    ])
    
    # 反归一化矩阵: [-1, 1] -> pixel
    S_moving_inv = np.array([
        [W_m / 2.0, 0, 0],
        [0, H_m / 2.0, 0],
        [0, 0, 1]
    ])
    
    return S_fixed @ matrix_3x3 @ S_moving_inv
```

**UI 交互：**
- 新增 `chkNormalizedMatrix` 复选框，位于 `chkOriginTopLeft` 下方
- 仅在「中心原点」模式下可用，选择「左上角原点」时自动禁用
- 默认启用归一化矩阵模式

### 实现

#### 前端
- `mainwindow.ui`: 添加 `chkNormalizedMatrix` 复选框
- `mainwindow.h`: 添加 `m_useNormalizedMatrix` 成员变量
- `mainwindow.cpp`: 初始化和信号连接，计算/预览时传递参数
- `AppConfig.h/.cpp`: 添加 `optionNormalizedMatrix()` 配置项
- `BackendClient.h/.cpp`: 更新 `computeRigid()` 和 `requestCheckerboardPreview()` 接口

#### 后端
- `schemas.py`: 添加 `use_normalized_matrix`、`fixed_image_size`、`moving_image_size` 字段
- `transforms.py`: 添加 `normalize_matrix()` 和 `denormalize_matrix()` 函数
- `server.py`: 处理归一化选项，返回对应格式的矩阵
- `warp_utils.py`: `create_affine_grid()` 支持直接使用归一化矩阵

#### 预览逻辑
当使用归一化矩阵时，预览将矩阵直接用于 grid_sample，无需额外坐标转换。

### 修改文件
- `frontend/mainwindow.ui`
- `frontend/mainwindow.h`
- `frontend/mainwindow.cpp`
- `frontend/app/AppConfig.h`
- `frontend/app/AppConfig.cpp`
- `frontend/app/BackendClient.h`
- `frontend/app/BackendClient.cpp`
- `backend/rigidlabeler_backend/api/schemas.py`
- `backend/rigidlabeler_backend/api/server.py`
- `backend/rigidlabeler_backend/core/transforms.py`
- `backend/rigidlabeler_backend/core/warp_utils.py`

---

## #028 - 2025-12-05

### 问题
`matrix_to_affine_params()` 和 `compute_affine_transform()` 中使用 SVD 分解提取仿射参数（旋转角、缩放、剪切）时，计算结果不正确：
1. 旋转角显示为补角（如 178° 而非 -2°）
2. Scale 和 Shear 的含义不符合标准仿射分解模型

### 分析
SVD 分解 `A = U @ Σ @ V^T` 得到的奇异值总是正的且按大小排序，无法正确表达原始变换的 scale_x/scale_y 及 shear。

正确的分解模型应为：
```
A = R @ [[sx, shear*sy], [0, sy]]
```
其中 R 是旋转矩阵，右边是上三角的缩放+剪切矩阵。

### 解决方案
改用 **QR 分解**：`A = Q @ R`
- Q：正交矩阵（旋转）
- R：上三角矩阵，直接读取 scale 和 shear

### 实现

```python
def matrix_to_affine_params(matrix_3x3: np.ndarray):
    A = matrix_3x3[0:2, 0:2]
    tx = matrix_3x3[0, 2]
    ty = matrix_3x3[1, 2]

    # QR 分解
    Q, R = np.linalg.qr(A)

    # 确保 Q 是纯旋转（det > 0）
    if np.linalg.det(Q) < 0:
        Q[:, 1] *= -1
        R[1, :] *= -1

    # 从 R 读取 scale
    sx = R[0, 0]
    sy = R[1, 1]

    # 处理负 scale，吸收进 Q
    if sx < 0:
        sx = -sx
        Q[:, 0] *= -1
        R[0, :] *= -1
    if sy < 0:
        sy = -sy
        Q[:, 1] *= -1
        R[1, :] *= -1

    # 在处理完负 scale 后计算旋转角（关键！）
    theta_rad = np.arctan2(Q[1, 0], Q[0, 0])
    theta_deg = np.degrees(theta_rad)

    # Shear
    shear = R[0, 1] / sy if abs(sy) > 1e-10 else 0.0

    return theta_deg, tx, ty, sx, sy, shear
```

### 关键修复
**旋转角必须在处理完负 scale 之后计算**，否则翻转 Q 的列会导致角度偏差 180°。

### 修改文件
- `backend/rigidlabeler_backend/core/transforms.py`
  - `compute_affine_transform()` - 仿射变换计算时的参数分解
  - `matrix_to_affine_params()` - 矩阵转参数的工具函数

---

## #027 - 2025-12-04

### 需求
统一撤销系统，将原来的两套独立撤销栈合并为一套，使 Ctrl+Z 按操作时间顺序撤销，符合用户直觉。

### 问题分析
原有两套独立系统：
1. `TiePointModel` 内置的 `m_undoStack`/`m_redoStack` - 用于撤销添加点
2. `MainWindow` 的 `QUndoStack` - 用于撤销删除点

问题：用户按时间顺序执行 `添加→添加→删除→添加`，但 Ctrl+Z 会先撤销所有「添加」，再撤销「删除」，不符合直觉。

### 解决方案
统一使用 `QUndoStack`（Qt Command Pattern），所有操作封装为 `QUndoCommand`。

### 实现

#### 新增 AddPointCommand 类
```cpp
class AddPointCommand : public QUndoCommand {
    void redo() override {
        m_pairIndex = m_model->addFixedPointDirect(m_position);  // 或 addMovingPointDirect
    }
    void undo() override {
        m_model->removePointDirect(m_pairIndex, m_isFixed);
    }
};
```

#### TiePointModel 修改

1. 移除内置撤销栈 (`m_undoStack`, `m_redoStack`)
2. 移除 `canUndo()`, `canRedo()`, `undoLastPoint()`, `redoLastPoint()` 方法
3. 移除 `undoRedoStateChanged` 信号
4. 新增直接操作方法（供 Command 调用）：
   - `addFixedPointDirect(const QPointF &point)` - 自动分配 pairIndex
   - `addFixedPointDirect(int pairIndex, const QPointF &point)` - 指定 pairIndex
   - `addMovingPointDirect(...)` - 同上
   - `removePointDirect(int pairIndex, bool isFixed)` - 移除单个点

#### MainWindow 修改

1. 点击添加点时创建 `AddPointCommand` 并 push 到 `QUndoStack`
2. 简化 `undo()`/`redo()` 函数，只使用 `QUndoStack`
3. 更新 `updateActionStates()`，只检查 `QUndoStack` 状态

### 效果
- ✅ 按操作时间顺序撤销，符合用户直觉
- ✅ 代码简化，只维护一套系统
- ✅ 状态栏显示撤销/重做的操作名称

---

## #026 - 2025-12-04

### 问题
Ctrl+Z 撤销删除点对操作后，恢复的点编号（pairIndex）与原来不一致。

### 分析
`insertTiePoint()` 方法在恢复点时使用 `getNextPairIndex()` 分配新编号，而不是恢复原来的编号。`RemoveTiePointCommand` 也没有保存原始的 `pairIndex`。

### 修复

#### TiePointModel 修改

1. `insertTiePoint()` 添加可选的 `pairIndex` 参数：
```cpp
void insertTiePoint(int index, const QPointF &fixed, const QPointF &moving, int pairIndex = -1);
```

2. 添加 `getPairIndexAt()` 方法获取指定行的 pairIndex：
```cpp
int TiePointModel::getPairIndexAt(int index) const
{
    if (index < 0 || index >= m_pairs.count())
        return -1;
    return m_pairs.at(index).index;
}
```

#### RemoveTiePointCommand 修改

保存并恢复原始的 pairIndex：
```cpp
RemoveTiePointCommand(...) {
    // ...
    m_pairIndex = m_model->getPairIndexAt(index);  // 保存原始编号
}

void undo() override {
    m_model->insertTiePoint(m_index, m_fixed, m_moving, m_pairIndex);  // 恢复时使用原编号
}
```

---

## #025 - 2025-12-04

### 问题
关闭前设置语言为中文，重新打开程序后虽然设置中的语言选项显示为中文，但整个界面仍然是英文。

### 分析
在构造函数中，`setupUi(ui)` 先执行，此时界面用英文初始化。之后恢复语言设置时虽然加载了中文翻译器 (`qApp->installTranslator(m_translator)`)，但没有调用 `ui->retranslateUi(this)` 来重新翻译界面元素。

### 修复
在成功安装翻译器后，立即调用 `ui->retranslateUi(this)` 更新界面文本：

```cpp
if (m_translator->load(translationPath) || m_translator->load("rigidlabeler_zh", ":/translations")) {
    qApp->installTranslator(m_translator);
    // Re-translate the UI after installing translator
    ui->retranslateUi(this);  // 添加：重新翻译界面
}
```

---

## #024 - 2025-12-04

### 问题
Ctrl+Z 撤销删除点对操作后，数据模型中的点被恢复，但界面上的十字标记没有重新绘制。

### 分析
`undo()` 和 `redo()` 函数在通过 `QUndoStack` 执行撤销/重做时，没有调用 `updatePointDisplay()` 来刷新界面显示。

### 修复
在 `m_undoStack->undo()` 和 `m_undoStack->redo()` 调用后添加界面刷新：

```cpp
// undo()
if (m_undoStack->canUndo()) {
    m_undoStack->undo();
    m_hasValidTransform = false;
    updatePointDisplay();  // 添加：刷新点标记显示
    updateActionStates();
    statusBar()->showMessage(tr("Undo: Tie point(s) restored"), 2000);
}

// redo()
if (m_undoStack->canRedo()) {
    m_undoStack->redo();
    m_hasValidTransform = false;
    updatePointDisplay();  // 添加：刷新点标记显示
    updateActionStates();
    statusBar()->showMessage(tr("Redo: Tie point(s) deleted"), 2000);
}
```

---

## #023 - 2025-12-02

### 问题
点击单独的"上一张Fixed"/"下一张Fixed"/"上一张Moving"/"下一张Moving"按钮时不会清除控制点和变换结果，而"Next Pair"按钮会清除。

### 分析
`prevPair()` 和 `nextPair()` 函数包含清除逻辑，但各个独立的图像导航函数 (`prevFixedImage()`、`nextFixedImage()`、`prevMovingImage()`、`nextMovingImage()`) 以及加载函数 (`loadFixedImage()`、`loadMovingImage()`) 没有清除逻辑。

### 修复

#### 修改导航函数
为所有 4 个导航函数添加相同的清除逻辑：
```cpp
void MainWindow::prevFixedImage()
{
    if (m_fixedImageIndex <= 0)
        return;
    
    // 切换前保存当前控制点
    saveProjectState();
    
    // 清除当前控制点和变换结果
    m_tiePointModel->clearAll();
    m_hasValidTransform = false;
    ui->txtResult->clear();
    
    loadFixedImageByIndex(m_fixedImageIndex - 1);
}
```

#### 修改加载函数
`loadFixedImage()` 和 `loadMovingImage()` 对话框加载成功后也执行清除：
```cpp
if (m_imagePairModel->loadFixedImage(fileName)) {
    // ... 更新索引和状态 ...
    
    // 清除控制点和变换结果
    m_tiePointModel->clearAll();
    m_hasValidTransform = false;
    ui->txtResult->clear();
}
```

#### 重构 Pair 导航
`prevPair()` 和 `nextPair()` 改为直接调用 `loadXxxImageByIndex()` 而不是调用导航函数，避免重复清除：
```cpp
void MainWindow::nextPair()
{
    // 检查是否可以前进
    bool canNextFixed = ...;
    bool canNextMoving = ...;
    if (!canNextFixed && !canNextMoving)
        return;
    
    saveProjectState();
    m_tiePointModel->clearAll();
    m_hasValidTransform = false;
    ui->txtResult->clear();
    
    // 直接加载，不再调用导航函数
    if (canNextFixed) {
        loadFixedImageByIndex(m_fixedImageIndex + 1);
    }
    if (canNextMoving) {
        loadMovingImageByIndex(m_movingImageIndex + 1);
    }
}
```

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
