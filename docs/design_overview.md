# RigidLabeler – 设计总览（Design Overview）

版本：`v0.1.0`

## 1. 目标与范围（Goal & Scope）

RigidLabeler 是一个轻量级桌面工具，用于给一对二维图像之间的 **刚性变换（或相似变换）打标签**。  

典型使用场景包括：

- 在两张图像之间（例如 IR–VIS）**手动标注对应点（tie points）**；
- 基于这些点对，计算 2D 刚性/相似变换参数：
  - 旋转角 `θ`
  - 平移 `(tx, ty)`
  - 可选统一缩放 `s`
- 将这些参数（以及矩阵与点对）导出为标签，用于：
  - 深度学习训练数据标注
  - 算法评估与定量对比
  - 自建配准数据集管理

本版本（v0.1.0）的范围：

- 单机、本地使用的桌面工具（无多用户/权限等复杂需求）；
- 前端使用 **Qt C++** 实现桌面 GUI；
- 后端使用 **FastAPI** 提供本地 HTTP JSON 服务，在 `localhost` 上运行。

---

## 2. 整体架构（High-Level Architecture）

RigidLabeler 采用一个非常简单的 **两层架构**：

```text
+----------------------------+        HTTP / JSON       +----------------------------+
|        Qt 前端（GUI）      |  <-------------------->  |      FastAPI 后端服务       |
|                            |                          |                            |
|  - MainWindow              |                          |  - /health                 |
|  - 固/动图像视图           |                          |  - /compute/rigid          |
|  - Tie Point 表格          |                          |  - /labels/save            |
|  - 工具栏 / 状态栏         |                          |  - /labels/load            |
|  - BackendClient (HTTP)    |                          |  - (/labels/list)          |
+----------------------------+                          |  - (/warp/preview)         |
                                                       +----------------------------+
````

### 2.1 前端职责（Qt）

前端主要负责：

* 加载和显示一对图像（固定图 / 移动图）；
* 交互式创建、编辑、删除 **对应点（tie points）**；
* 调用后端 HTTP 接口：

  * 计算刚性变换；
  * 保存 / 加载标签；
  * （可选）生成 warp 预览图；
* 维护当前会话的内存状态：

  * 当前图像对；
  * 当前点对列表；
  * 当前变换参数与标签信息。

### 2.2 后端职责（FastAPI）

后端主要负责：

* 提供一组干净的 HTTP/JSON 接口（FastAPI）；
* 实现刚性变换的数值计算逻辑；
* 计算变换矩阵与误差指标；
* 将标签持久化（JSON/CSV/NPY 等）到磁盘；
* （可选）利用图像库（如 OpenCV）生成 warp 预览图。

---

## 3. 功能总览（Functional Overview）

下面按 **前端功能块** 和 **后端功能块** 列出 v0.1.0 需要实现的主要功能。

### 3.1 前端 – UI 与交互

#### F1. 图像对管理（Image Pair Management）

* **F1.1 加载固定图像**

  * 通过文件对话框选择固定图像；
  * 在左侧视图显示；
  * 将路径存入 `ImagePairModel`。

* **F1.2 加载移动图像**

  * 通过文件对话框选择移动图像；
  * 在右侧视图显示；
  * 将路径存入 `ImagePairModel`。

* **F1.3 清除当前图像对**

  * 清空两个视图；
  * 清空当前 tie points 和变换结果；
  * 重置内部状态。

* **F1.4 工程路径 / 数据根目录配置**

  * 从 `config/app.yaml` 读取默认数据目录；
  * （可选）允许用户在 UI 中修改当前工程或数据目录。

#### F2. 图像浏览与导航（Image Viewing & Navigation）

* **F2.1 基础浏览能力**

  * 缩放（鼠标滚轮 / 工具栏按钮）；
  * 平移（鼠标拖拽）；
  * 适应窗口 / 1:1 显示模式切换。

* **F2.2 视图联动**

  * 提供一个开关：联动 fixed / moving 视图；

    * 联动缩放；
    * 联动平移；
  * 方便对比细节，寻找对应点。

#### F3. 对应点标注与编辑（Tie Point Creation & Editing）

* **F3.1 交互式添加 Tie Point**

  * 用户在固定图像上点击，记录 `fixed = (x_f, y_f)`；
  * 用户在移动图像上点击，记录 `moving = (x_m, y_m)`；
  * 两次点击后创建一个 `TiePoint`，并添加到列表；
  * 在两个视图上用标记显示该点对。

* **F3.2 Tie Point 表格展示**

  * 表格显示所有点对：序号、fixed.x、fixed.y、moving.x、moving.y；
  * 允许在表格中直接修改坐标数值；
  * 选中某行时，在视图中高亮对应点。

* **F3.3 删除 / 清空 Tie Points**

  * 删除当前选中点对；
  * 一键清空该图像对的所有点对。

* **F3.4 UI 级基本校验**

  * 如果点对数量少于变换计算所需阈值，在 UI 上给出提示；
  * 坐标明显越界时，可在 UI 层做简单警告（可选）。

* **F3.5 撤销 / 重做（可选，未来）**

  * 对点对增删改进行简单的历史记录；
  * 支持 Undo / Redo 操作。

#### F4. 变换计算流程（Transform Computation Workflow）

* **F4.1 触发刚性变换计算**

  * “Compute Transform” 按钮；
  * 收集当前：

    * `image_fixed`、`image_moving`；
    * `tie_points`；
    * `allow_scale` 标志；
  * 调用后端 `POST /compute/rigid`。

* **F4.2 展示计算结果**

  * 在 UI 中显示：

    * `θ`（度）、`tx`、`ty`、`scale`；
    * RMS 误差；
  * 可在只读文本框中显示 3×3 矩阵。

* **F4.3 错误处理**

  * 当后端返回 `status="error"` 时，在对话框或状态栏显示错误信息；
  * 对常见错误码提供友好提示：

    * `NOT_ENOUGH_POINTS`：点数不足；
    * `SINGULAR_TRANSFORM`：几何退化；
    * `INVALID_INPUT`：输入非法等。

#### F5. 标签保存 / 加载（Label Save / Load）

* **F5.1 保存标签**

  * “Save Label” 按钮；
  * 收集当前标签状态：

    * `image_fixed`、`image_moving`；
    * `rigid` 参数；
    * `matrix_3x3`；
    * `tie_points`；
    * `meta`（时间戳、备注等）；
  * 调用 `POST /labels/save`；
  * 将后端返回的保存路径 / label_id 显示给用户。

* **F5.2 加载标签**

  * 用户选择图像对后，可点击“Load Label”；
  * 调用 `GET /labels/load`，参数为 `image_fixed` & `image_moving`；
  * 若后端返回标签：

    * 恢复 `rigid` 参数与矩阵；
    * 恢复 tie points 列表；
    * 同步更新视图展示；
  * 若不存在标签：

    * 显示“未找到标签”的友好提示。

* **F5.3 标签列表浏览（可选）**

  * 在某个“标签浏览器”窗口中：

    * 调用 `GET /labels/list` 列出当前工程下的所有标签；
    * 用户选择某个标签后：

      * 加载其对应图像对；
      * 加载其 tie points 与变换。

#### F6. 预览与可视化（可选）

* **F6.1 请求 Warp 预览**

  * 当已有变换结果时，用户点击 “Preview Warp”；
  * 将刚性参数或 3×3 矩阵发给 `POST /warp/preview`；
  * 接收 `preview_path`。

* **F6.2 显示预览图**

  * 前端加载 `preview_path` 指向的 warp 后图像；
  * 可叠加显示 fixed 与 warped moving，并提供透明度滑条等。

#### F7. 后端连接与状态（Backend Connectivity）

* **F7.1 健康检查**

  * 程序启动时调用 `GET /health`：

    * 若成功，显示后端版本与“在线”状态；
    * 可在状态栏显示一个绿色/红色指示灯；
  * 也可在用户点击“重新检测”时主动调用。

* **F7.2 HTTP 异常处理**

  * 当后端不可达 / 返回非 JSON / 超时时：

    * 显示网络或后端错误信息；
    * 临时禁用“计算变换”、“保存标签”等按钮，避免误操作。

---

### 3.2 后端 – 核心逻辑与服务（Backend Core & Services）

#### B1. 健康与诊断（Health & Diagnostics）

* **B1.1 健康检查接口**

  * `GET /health`：

    * 返回当前版本号、后端类型（fastapi）等基本信息；
    * 前端用于判断后端是否正常运行。

#### B2. 刚性变换估计（Rigid Transform Estimation）

* **B2.1 输入校验**

  * 校验请求体结构：

    * `tie_points` 不为空；
    * 数量 ≥ 设定的最小点数要求；
  * 校验数值有效性（非 NaN、非 inf）。

* **B2.2 计算刚性变换**

  * 在 `core/transforms.py` 实现：

    * 计算 fixed 与 moving 点的质心；
    * 将点坐标分别减去质心，得到中心化坐标；
    * 计算协方差矩阵并进行 SVD（Procrustes 方法）：

      * `R = U V^T`；
      * 调整符号确保 det(R) = +1；
    * 若 `allow_scale = true`：

      * 从 trace 或 Frobenius 范数计算统一缩放 `scale`；
    * 计算平移：

      * `t = μ_fixed - scale * R * μ_moving`；
    * 最终导出：

      * 旋转角 `theta_deg`；
      * 平移 `tx`, `ty`；
      * `scale`。

* **B2.3 构建 3×3 矩阵**

  * 构造齐次坐标下的矩阵：

    ```text
    [ sR_11  sR_12  tx ]
    [ sR_21  sR_22  ty ]
    [ 0      0      1  ]
    ```
  * 以行主序形式返回 JSON。

* **B2.4 误差指标**

  * 用估计的变换将 moving 点映射到 fixed 坐标系；
  * 计算与真实 fixed 点之间的残差；
  * 返回：

    * RMS 误差（整体误差）；
    * （未来可扩展）每个点的残差列表。

* **B2.5 错误处理**

  * 几何退化 / 矩阵奇异：

    * 返回 `status="error"` 与 `error_code="SINGULAR_TRANSFORM"`；
  * 点数不足：

    * 返回 `error_code="NOT_ENOUGH_POINTS"`。

#### B3. 标签存储（Label Storage）

* **B3.1 保存标签**

  * 在 `io/label_store.py` 中实现：

    * 根据请求构建 `Label` 对象；
    * 根据 `image_fixed` & `image_moving` 生成唯一 label id 与文件名；
    * 将标签写入配置的 `labels_root` 目录下；
    * 使用 JSON（后续可扩展 CSV/NPY）。
    * 处理目录不存在 / 权限问题等 IO 错误。

* **B3.2 加载标签**

  * 根据 `image_fixed` & `image_moving`：

    * 映射到对应的标签文件路径；
    * 若存在：

      * 读取文件并反序列化为 `Label`；
    * 若不存在：

      * 返回 `status="error"`，`error_code="LABEL_NOT_FOUND"`。

* **B3.3 标签列表（可选）**

  * 扫描 `labels_root` 目录；
  * 从文件名或内容中解析出：

    * `label_id`；
    * `image_fixed`；
    * `image_moving`；
  * 返回 `LabelListItem` 列表，供前端浏览。

#### B4. Warp 预览（可选）

* **B4.1 图像读取**

  * 在 `io/image_loader.py` 中实现：

    * 使用 OpenCV / PIL 读入 moving 图像；
    * 读入 fixed 图像用于确定输出尺寸。

* **B4.2 生成 warp 图**

  * 根据 3×3 变换矩阵：

    * 对 moving 图进行 warp：

      * 刚性/相似变换可用 `warpAffine`；
      * 更通用的 3×3 可用 `warpPerspective`；
    * 输出尺寸对齐 fixed 图像。

* **B4.3 保存预览文件**

  * 将 warp 后的图像保存到例如 `data/temp/preview_*.png`；
  * 返回 `preview_path` 字段给前端。

#### B5. 配置与日志（Config & Logging）

* **B5.1 配置读取**

  * 在 `config.py` 中：

    * 读取 `config/backend.yaml`；
    * 提供：

      * `data_root`；
      * `labels_root`；
      * `temp_root`；
      * 服务器 `host` / `port` 等。

* **B5.2 日志记录**

  * 在 `utils/logging_utils.py` 中初始化日志系统；
  * 对以下内容进行记录：

    * 基本请求信息；
    * 错误 / 异常栈；
    * 统计信息（如估计的旋转分布等，可选）。

* **B5.3 全局异常处理（可选）**

  * FastAPI 中注册异常处理器 / 中间件；
  * 捕捉未处理异常，统一返回 JSON：

    * `status="error"`；
    * `error_code="INTERNAL_ERROR"`。

---

## 4. 数据与标签模型（Data & Label Model）

核心实体：

* **ImagePair**

  * `image_fixed`
  * `image_moving`

* **TiePoint**

  * `fixed: Point2D`
  * `moving: Point2D`

* **RigidParams**

  * `theta_deg`
  * `tx`, `ty`
  * `scale`

* **Label**

  * `image_fixed`, `image_moving`
  * `rigid`
  * `matrix_3x3`
  * `tie_points[]`
  * `meta`

这些模型在前后端之间共享概念：

* 后端：作为 Pydantic schema 和内部业务模型；
* 前端：作为 C++ 结构体 / 模型，通过 JSON 序列化/反序列化进行交互。

---

## 5. 典型工作流（Typical Workflows）

### 5.1 手工标注工作流

1. 用户启动前端，后端进程已启动或由前端启动；
2. 前端调用 `/health`，确定后端在线；
3. 用户分别加载固定图像与移动图像；
4. 用户在 UI 上交互式点选 / 编辑 tie points；
5. 用户点击 “Compute Transform”：

   * 前端调用 `/compute/rigid`；
   * 后端返回刚性参数 + 矩阵 + RMS 误差；
   * 前端展示结果；
6. 用户确认满意 → 点击 “Save Label”：

   * 前端调用 `/labels/save`；
   * 后端将标签写入 `labels/` 目录；
7. 下次打开同一对图像：

   * 前端调用 `/labels/load`；
   * 若存在标签，则自动恢复 tie points 与变换结果。

### 5.2 Warp 预览工作流（可选）

1. 在完成步骤 1–5 后，用户点击 “Preview Warp”；
2. 前端调用 `/warp/preview`，传递当前变换信息；
3. 后端生成 warp 后的移动图像，返回 `preview_path`；
4. 前端加载预览图，与固定图叠加显示（可加透明度调节）。

---

## 6. 非功能性需求（Non-Functional Requirements，v0.1.0）

* **性能**

  * 刚性变换估计：在典型点数（N ≤ 100）下单次计算应在 10 ms 以内完成；
  * Warp 预览：对 512×512 ~ 1024×1024 图像，预览生成应在几百毫秒量级。

* **可靠性**

  * 遇到文件缺失、路径错误等情况应给出清晰错误信息；
  * 避免因退化点集导致后端崩溃。

* **可移植性**

  * 目标平台：

    * Windows 10/11；
    * （可选）Linux。

* **可测试性**

  * 对核心模块编写单元测试：

    * `core/transforms.py`：已知输入对应已知输出的测试；
    * `io/label_store.py`：保存-加载的一致性测试；
    * API 层：使用 FastAPI 的 TestClient 测试各接口。

* **可扩展性**

  * 后续可增加：

    * 更多变换类型（仿射、单应等）；
    * 批量标注和批处理接口；
    * 与更大规模模型管理 / 训练平台的集成。

