# RigidLabeler Backend API Spec

Version: `v1.0.1`  
Base URL: `http://127.0.0.1:8000`

## 1. 总体约定

### 1.1 通信模式

- 协议：HTTP/1.1
- 数据格式：JSON
- 编码：UTF-8
- 客户端：Qt C++（通过 `QNetworkAccessManager` 调用）
- 服务器：FastAPI + Uvicorn

### 1.2 坐标系约定

- 所有坐标单位均为 **像素**。
- `Point2D` 中：
  - `x`：列坐标（从左到右递增）
  - `y`：行坐标（从上到下递增）
- 坐标以 **图像左上角** 为原点，右向为 +x，下向为 +y。

> **前端坐标模式说明**：
> 前端支持两种坐标显示/导出模式：
> - **图像中心原点**（默认）：(0,0) 位于图像中心，便于直观理解偏移
> - **左上角原点**：传统像素坐标系
>
> 无论前端使用哪种显示模式，发送到后端 API 的坐标都会根据用户选择的模式进行转换，确保与后端的坐标系一致。

### 1.3 图像路径约定

- 字段 `image_fixed` / `image_moving` 为字符串路径。
- 推荐使用 **相对于数据根目录（例如 `data/images/`）的相对路径**，具体由 `backend.yaml` 中的配置决定。
- 后端负责将相对路径解析为实际文件系统路径。

### 1.4 响应格式与错误处理

所有接口统一返回一个**顶层响应对象**：

```json
{
  "status": "ok" | "error",
  "message": "optional human readable info",
  "error_code": "OPTIONAL_ERROR_CODE",
  "data": { ... }  // 根据不同接口而变化
}
```

* HTTP 状态码约定：

  * **200**：请求被成功处理（无论业务是成功还是失败）

    * `status = "ok"` 表示业务成功
    * `status = "error"` 表示业务失败（如点数不足、标签不存在等）
  * 其他状态码（4xx/5xx）：仅用于服务器异常、路由不存在等情况，Qt 端可以统一视为“后端不可用”。

* 常见 `error_code` 枚举（可扩展）：

  * `INVALID_INPUT`：请求参数非法
  * `NOT_ENOUGH_POINTS`：点对数量不足以估计刚性变换
  * `SINGULAR_TRANSFORM`：协方差矩阵奇异，无法估计稳定变换
  * `LABEL_NOT_FOUND`：未找到对应标签
  * `IO_ERROR`：文件读写错误
  * `INTERNAL_ERROR`：未分类的内部错误

---

## 2. 数据模型（Schemas）

以下为逻辑模型，用于 Pydantic 定义与协议说明。

### 2.1 Point2D

二维像素坐标。

```json
{
  "x": 120.5,
  "y": 80.2
}
```

字段说明：

* `x` *(float)*：列坐标
* `y` *(float)*：行坐标

---

### 2.2 TiePoint

固定图像与移动图像上的一对对应点。

```json
{
  "fixed": { "x": 120.5, "y": 80.2 },
  "moving": { "x": 130.1, "y": 75.9 }
}
```

字段说明：

* `fixed` *(Point2D)*：固定图像（`image_fixed`）上的点
* `moving` *(Point2D)*：移动图像（`image_moving`）上的点

---

### 2.3 RigidParams

2D 变换参数（支持刚性、相似、仿射三种模式）。

```json
{
  "theta_deg": 5.3,
  "tx": 12.4,
  "ty": -3.1,
  "scale_x": 1.05,
  "scale_y": 0.98,
  "shear": 0.02
}
```

字段说明：

* `theta_deg` *(float)*：逆时针旋转角度，单位为度
* `tx` *(float)*：x 方向平移（列方向）
* `ty` *(float)*：y 方向平移（行方向）
* `scale_x` *(float)*：x 方向缩放因子，默认为 1.0
* `scale_y` *(float)*：y 方向缩放因子，默认为 1.0
* `shear` *(float)*：剪切因子，默认为 0.0

> **变换模式说明**：
> - **Rigid（刚性）**：仅使用 `theta_deg`, `tx`, `ty`，`scale_x = scale_y = 1.0`, `shear = 0`
> - **Similarity（相似）**：使用 `theta_deg`, `tx`, `ty`, `scale_x = scale_y = scale`
> - **Affine（仿射）**：使用全部 6 个参数

---

### 2.4 Label

完整标签对象，用于保存/加载一对图像的配准信息。

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
    {
      "fixed":  { "x": 120.5, "y": 80.2 },
      "moving": { "x": 130.1, "y": 75.9 }
    }
  ],
  "meta": {
    "comment": "manually labeled by user",
    "timestamp": "2025-11-30T15:00:00"
  }
}
```

字段说明：

* `image_fixed` *(string)*：固定图像路径
* `image_moving` *(string)*：移动图像路径
* `rigid` *(RigidParams)*：刚性变换参数
* `matrix_3x3` *(float[3][3])*：齐次坐标系下的 3×3 变换矩阵
* `tie_points` *(TiePoint[])*：用于估计该变换的点对列表
* `meta` *(object, optional)*：附加元信息（备注、时间戳等）

---

### 2.5 常用响应数据模型

#### 2.5.1 HealthInfo

```json
{
  "version": "1.0.1",
  "backend": "fastapi"
}
```

#### 2.5.2 ComputeRigidResult

```json
{
  "rigid": { ... },        // RigidParams
  "matrix_3x3": [ ... ],   // 3x3 matrix
  "rms_error": 0.82,
  "num_points": 10
}
```

字段说明：

* `rigid` *(RigidParams)*：刚性参数
* `matrix_3x3` *(float[3][3])*：变换矩阵
* `rms_error` *(float)*：在输入点对上的 RMS 残差
* `num_points` *(int)*：参与估计的点对数量

#### 2.5.3 LabelSaveResult

```json
{
  "label_path": "data/labels/2a1f9c8e_vis001_ir001.json",
  "label_id": "2a1f9c8e"
}
```

#### 2.5.4 LabelListItem

```json
{
  "label_id": "2a1f9c8e",
  "label_path": "data/labels/2a1f9c8e_vis001_ir001.json",
  "image_fixed": "data/images/vis_001.png",
  "image_moving": "data/images/ir_001.png"
}
```

#### 2.5.5 WarpPreviewResult

```json
{
  "preview_path": "data/temp/preview_vis001_ir001.png"
}
```

---

## 3. 接口定义

### 3.1 `GET /health`

**用途**：健康检查、调试，确认后端在线。

* **Method**: `GET`
* **Path**: `/health`
* **Headers**:

  * `Accept: application/json`
* **Query Params**: 无
* **Request Body**: 无

#### Response (示例)

```json
{
  "status": "ok",
  "message": "rigidlabeler backend alive",
  "error_code": null,
  "data": {
    "version": "1.0.1",
    "backend": "fastapi"
  }
}
```

---

### 3.2 `POST /compute/rigid`

**用途**：根据给定点对列表，计算 2D 变换（支持刚性、相似、仿射三种模式）。

* **Method**: `POST`
* **Path**: `/compute/rigid`
* **Headers**:

  * `Content-Type: application/json`
  * `Accept: application/json`

#### Request Body

```json
{
  "image_fixed": "data/images/vis_001.png",
  "image_moving": "data/images/ir_001.png",
  "tie_points": [
    {
      "fixed":  { "x": 120.5, "y": 80.2 },
      "moving": { "x": 130.1, "y": 75.9 }
    },
    {
      "fixed":  { "x": 200.0, "y": 150.0 },
      "moving": { "x": 210.3, "y": 143.2 }
    },
    {
      "fixed":  { "x": 80.0, "y": 220.0 },
      "moving": { "x": 85.5, "y": 215.8 }
    }
  ],
  "transform_mode": "affine",
  "min_points_required": 3
}
```

字段说明：

* `image_fixed` *(string)*：固定图像路径（用于后续一致性检查或日志记录，可选）
* `image_moving` *(string)*：移动图像路径
* `tie_points` *(TiePoint[])*：点对列表，必须 >= `min_points_required`
* `transform_mode` *(string)*：变换模式，可选值：
  * `"rigid"`：刚性变换（旋转 + 平移），最少 2 个点
  * `"similarity"`：相似变换（旋转 + 平移 + 均匀缩放），最少 2 个点
  * `"affine"`：仿射变换（旋转 + 平移 + 非均匀缩放 + 剪切），最少 3 个点
* `allow_scale` *(bool, deprecated)*：已弃用，请使用 `transform_mode`
* `min_points_required` *(int)*：若输入点对数小于此值，返回错误

#### Success Response (status = ok)

```json
{
  "status": "ok",
  "message": null,
  "error_code": null,
  "data": {
    "rigid": {
      "theta_deg": 5.3,
      "tx": 12.4,
      "ty": -3.1,
      "scale_x": 1.05,
      "scale_y": 0.98,
      "shear": 0.02
    },
    "matrix_3x3": [
      [1.042, -0.117, 12.4],
      [0.092,  0.978, -3.1],
      [0.0,    0.0,   1.0]
    ],
    "rms_error": 0.82,
    "num_points": 10
  }
}
```

#### Error Response (status = error)

示例 1：点数不够

```json
{
  "status": "error",
  "message": "not enough points to estimate rigid transform (got 1, need at least 2)",
  "error_code": "NOT_ENOUGH_POINTS",
  "data": null
}
```

示例 2：矩阵奇异 / 算法失败

```json
{
  "status": "error",
  "message": "failed to estimate rigid transform: singular covariance matrix",
  "error_code": "SINGULAR_TRANSFORM",
  "data": null
}
```

---

### 3.3 `POST /labels/save`

**用途**：保存一对图像的标签（刚性参数 + 矩阵 + 点对）。

* **Method**: `POST`
* **Path**: `/labels/save`
* **Headers**:

  * `Content-Type: application/json`
  * `Accept: application/json`

#### Request Body

使用完整 `Label` 结构：

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
    {
      "fixed":  { "x": 120.5, "y": 80.2 },
      "moving": { "x": 130.1, "y": 75.9 }
    }
  ],
  "meta": {
    "comment": "manually labeled by wbh",
    "timestamp": "2025-11-30T15:00:00"
  }
}
```

#### Success Response

```json
{
  "status": "ok",
  "message": "label saved",
  "error_code": null,
  "data": {
    "label_path": "data/labels/2a1f9c8e_vis001_ir001.json",
    "label_id": "2a1f9c8e"
  }
}
```

#### Error Response

```json
{
  "status": "error",
  "message": "failed to save label: permission denied",
  "error_code": "IO_ERROR",
  "data": null
}
```

---

### 3.4 `GET /labels/load`

**用途**：根据图像路径加载对应标签。

* **Method**: `GET`
* **Path**: `/labels/load`
* **Headers**:

  * `Accept: application/json`

#### Query Parameters

* `image_fixed` *(string, required)*
* `image_moving` *(string, required)*

示例：

```text
GET /labels/load?image_fixed=data/images/vis_001.png&image_moving=data/images/ir_001.png
```

#### Success Response

```json
{
  "status": "ok",
  "message": null,
  "error_code": null,
  "data": {
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
      {
        "fixed":  { "x": 120.5, "y": 80.2 },
        "moving": { "x": 130.1, "y": 75.9 }
      }
    ],
    "meta": {
      "comment": "manually labeled by wbh",
      "timestamp": "2025-11-30T15:00:00"
    }
  }
}
```

#### Error Response（标签不存在）

```json
{
  "status": "error",
  "message": "label not found for given image pair",
  "error_code": "LABEL_NOT_FOUND",
  "data": null
}
```

---

### 3.5 `GET /labels/list`（可选）

**用途**：列出当前工程下已有的标签文件。

* **Method**: `GET`
* **Path**: `/labels/list`
* **Headers**:

  * `Accept: application/json`

#### Query Parameters（可选）

* `project` *(string, optional)*：工程名称或子目录名（如果你未来引入工程概念）

#### Response

```json
{
  "status": "ok",
  "message": null,
  "error_code": null,
  "data": [
    {
      "label_id": "2a1f9c8e",
      "label_path": "data/labels/2a1f9c8e_vis001_ir001.json",
      "image_fixed": "data/images/vis_001.png",
      "image_moving": "data/images/ir_001.png"
    },
    {
      "label_id": "9dfe120a",
      "label_path": "data/labels/9dfe120a_vis002_ir002.json",
      "image_fixed": "data/images/vis_002.png",
      "image_moving": "data/images/ir_002.png"
    }
  ]
}
```

---

### 3.6 `POST /warp/preview`（可选）

**用途**：根据给定变换参数对移动图像进行 warp，生成预览图像文件。

* **Method**: `POST`
* **Path**: `/warp/preview`
* **Headers**:

  * `Content-Type: application/json`
  * `Accept: application/json`

#### Request Body

方式 A：通过 `rigid` 参数

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
  "output_name": "preview_vis001_ir001.png"
}
```

方式 B：通过 `matrix_3x3`

```json
{
  "image_fixed": "data/images/vis_001.png",
  "image_moving": "data/images/ir_001.png",
  "matrix_3x3": [
    [0.995, -0.099, 12.4],
    [0.099,  0.995, -3.1],
    [0.0,    0.0,   1.0]
  ],
  "output_name": "preview_vis001_ir001.png"
}
```

字段说明：

* `image_fixed` *(string)*：固定图像路径，用于确定输出尺寸
* `image_moving` *(string)*：移动图像路径
* `rigid` *(RigidParams)* 或 `matrix_3x3` *(float[3][3])*：二者至少提供一个
* `output_name` *(string, optional)*：输出文件名，不提供则由后端自动生成

#### Success Response

```json
{
  "status": "ok",
  "message": null,
  "error_code": null,
  "data": {
    "preview_path": "data/temp/preview_vis001_ir001.png"
  }
}
```

#### Error Response

```json
{
  "status": "error",
  "message": "failed to generate warp preview: image not found",
  "error_code": "IO_ERROR",
  "data": null
}
```

---

## 4. 版本管理与兼容性

* 当前版本：`v0.1.1`，主要用于单机桌面工具开发与自用。
* 若未来调整字段或接口：

  * 建议保留向后兼容，或通过 URL 前缀进行版本划分，例如：`/api/v1/compute/rigid`。
  * 建议在本文件开头维护 **变更日志（changelog）**。

---

## 5. TODO / 待扩展方向

* 增加简易认证机制（如本地 token），避免被其他进程误调用
* 支持批量计算与批量保存标签的接口
* 支持更多变换类型（仿射、透视等）的扩展字段
* 返回更多诊断信息（例如每个点的残差列表）

```

如果你愿意，下一步我可以按这个 `api_spec.md` 帮你生成：

- `backend/api/schemas.py` 里的 Pydantic 模型代码骨架  
- `backend/api/server.py` 里 FastAPI 路由的函数框架（先不写算法，实现时你只管往里填就行）
::contentReference[oaicite:0]{index=0}
```
