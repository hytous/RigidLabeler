# 标签格式与命名规范（label_format.md）

RigidLabeler 用于保存配准标签的核心实体为 **Label**。  
本文件定义：

1. Label JSON 文件内部结构  
2. 标签文件命名规则  
3. 查找 / 加载标签时的约定  

---

## 1. Label JSON 结构

### 1.1 顶层结构示例

```json
{
  "image_fixed": "D:/datasetA/vis/vis_001.png",
  "image_moving": "D:/datasetA/ir/ir_001.png",
  "rigid": {
    "theta_deg": 5.3,
    "tx": 12.4,
    "ty": -3.1,
    "scale_x": 1.05,
    "scale_y": 0.98,
    "shear": 0.02
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
    "comment": "manually labeled",
    "timestamp": "2025-11-30T15:00:00"
  }
}
````

### 1.2 字段详细说明

#### 1.2.1 `image_fixed` / `image_moving`

* 类型：`string`
* 含义：

  * `image_fixed`：固定图像（Base/Fixed Image）的路径；
  * `image_moving`：移动图像（Warp/Moving Image）的路径。
* 路径形式：

  * 可以是 **绝对路径**（如 `D:/datasetA/vis/vis_001.png`），也可以是 **相对路径**（如 `vis/vis_001.png`）；
  * 若使用相对路径，推荐以“某个数据集根目录”为参考（例如：当前项目的数据目录）。
* 后端在读取时，将这两个字符串视为“要加载的图像路径”，具体是否与某个根目录拼接由实现决定。

#### 1.2.2 `rigid` 对象

```json
"rigid": {
  "theta_deg": 5.3,
  "tx": 12.4,
  "ty": -3.1,
  "scale_x": 1.05,
  "scale_y": 0.98,
  "shear": 0.02
}
```

* 类型：对象
* 字段：

  * `theta_deg` *(float)*：逆时针旋转角度，单位为"度"；
  * `tx` *(float)*：x 方向平移（列方向，像素）；
  * `ty` *(float)*：y 方向平移（行方向，像素）；
  * `scale_x` *(float)*：x 方向缩放因子；
  * `scale_y` *(float)*：y 方向缩放因子；
  * `shear` *(float)*：剪切系数（沿 x 方向）。
* 向后兼容：

  * `scale` *(float, deprecated)*：旧版统一缩放因子，仍可读取，但新版使用 `scale_x`、`scale_y`。
* 坐标约定：

  * 坐标原点在图像左上角；
  * 向右为 x 正方向（列坐标），向下为 y 正方向（行坐标）；
  * 对于**刚性变换**，`scale_x = scale_y = 1.0`，`shear = 0`；
  * 对于**相似变换**，`scale_x = scale_y`，`shear = 0`。

#### 1.2.3 `matrix_3x3`

```json
"matrix_3x3": [
  [0.995, -0.099, 12.4],
  [0.099,  0.995, -3.1],
  [0.0,    0.0,   1.0]
]
```

* 类型：3×3 浮点数矩阵（JSON 中表示为“数组的数组”）

* 含义：齐次坐标系下的 2D 变换矩阵，遵从关系：
  [
  \begin{bmatrix} x_f [2pt] y_f [2pt] 1 \end{bmatrix}
  ===================================================

  \mathbf{M}
  \begin{bmatrix} x_m [2pt] y_m [2pt] 1 \end{bmatrix}
  ]
  其中：

  * ((x_m, y_m))：移动图像上的点；
  * ((x_f, y_f))：固定图像上的对应点；
  * 即 `matrix_3x3` 实现 **从移动图像坐标系 → 固定图像坐标系** 的映射。

* 对刚性/相似变换：
  [
  \mathbf{M} =
  \begin{bmatrix}
  sR_{11} & sR_{12} & t_x \
  sR_{21} & sR_{22} & t_y \
  0 & 0 & 1
  \end{bmatrix}
  ]
  其中 (s) 为 `scale`，(R) 为 2×2 旋转矩阵。

#### 1.2.4 `tie_points`

```json
"tie_points": [
  {
    "fixed":  { "x": 120.5, "y": 80.2 },
    "moving": { "x": 130.1, "y": 75.9 }
  }
]
```

* 类型：数组，每个元素是一个 TiePoint 对象；
* 每个 TiePoint：

  * `fixed`：固定图像上的点坐标；
  * `moving`：移动图像上的点坐标；
  * 均为 `{ "x": float, "y": float }` 形式。
* 坐标约定：

  * 坐标单位为像素；
  * 左上角为原点，向右为 x+，向下为 y+；
  * 允许使用浮点数坐标（即使 UI 显示为整数，保存时也可以是 float）。

#### 1.2.5 `meta`（可选）

```json
"meta": {
  "comment": "manually labeled",
  "timestamp": "2025-11-30T15:00:00"
}
```

* 类型：对象或 `null`
* 预留字段示例：

  * `comment`：用户备注；
  * `timestamp`：ISO8601 格式时间字符串，例如 `"2025-11-30T15:00:00"`。
* 要求：

  * 解析时应采用**宽容模式**：

    * 允许 `meta` 为空或缺失；
    * 允许存在额外字段，对未知字段直接忽略。

---

## 2. 标签文件命名规则

标签文件统一存放在后端配置的 `labels_root` 目录下。
为便于人类阅读与脚本处理，文件名基于图像文件名派生。

设：

* `fixed_path` = `image_fixed`
* `moving_path` = `image_moving`
* `fixed_stem`  = 去掉扩展名后的文件名部分，例如：

  * `D:/datasetA/vis/vis_001.png` → `vis_001`
* `moving_stem` = 同理，例如：

  * `D:/datasetA/ir/ir_001.png` → `ir_001`

对 `fixed_stem` 与 `moving_stem` 做如下“清洗”（仅影响文件名，不修改 JSON 内部）：

1. 将所有非字母数字字符替换为 `_`；
2. 将多个连续 `_` 压缩为一个 `_`；
3. 全部转换为小写。

记清洗后的结果为：

* `f`：来自 `fixed_stem`；
* `m`：来自 `moving_stem`。

### 2.1 标准文件名格式

```text
<labels_root>/<f>__<m>.json
```

示例：

* `image_fixed = "D:/datasetA/vis/vis_001.png"`
  → `fixed_stem = "vis_001"` → `f = "vis_001"`
* `image_moving = "D:/datasetA/ir/ir_001.png"`
  → `moving_stem = "ir_001"` → `m = "ir_001"`

标签文件名为：

```text
<data_root>/labels/vis_001__ir_001.json
```

（具体目录以 `backend.yaml` 中的 `labels_root` 为准）

### 2.2 `label_id` 约定

* 定义 `label_id` 为**不包含扩展名的文件名**：

```text
label_id = "<f>__<m>"
```

例：

* 文件 `vis_001__ir_001.json` 对应：

  * `label_id = "vis_001__ir_001"`。

若接口中需要返回 `label_id`（例如 `/labels/save` 的响应），应遵循此规则。

### 2.3 文件名冲突处理

在典型使用场景（同一对图像只维护一个标签）下，文件名冲突概率较低。
v0.1.1 版本策略：

* 若同一对图像重复保存标签，新标签会覆盖旧文件（相同 `<f>__<m>.json`）；
* 若需要多版本管理，可在更高版本中扩展为：

  * `vis_001__ir_001.v1.json`
  * `vis_001__ir_001.v2.json`
    等形式，本版本不实现。

---

## 3. 标签查找与加载规则

在 `GET /labels/load` 接口中，给定：

* `image_fixed`（请求参数）
* `image_moving`（请求参数）

后端进行如下操作：

1. 按 **2. 标签文件命名规则** 对 `image_fixed` 与 `image_moving` 计算 `f` 和 `m`；
2. 构造目标文件路径：

   ```text
   label_path = labels_root / (f + "__" + m + ".json")
   ```
3. 检查 `label_path` 是否存在：

   * 若存在：

     * 读取 JSON 文件；
     * 反序列化为 Label 对象；
     * 返回 `status="ok"` 与完整标签数据；
   * 若不存在：

     * 返回 `status="error"`，`error_code="LABEL_NOT_FOUND"`。

> 约定：
>
> * v0.1.1 不做模糊匹配和多版本选择，仅根据以上规则进行**一一对应查找**；
> * 若将来引入多版本标签，可通过 `meta` 中的版本字段或额外接口扩展支持。

---

## 4. 向前兼容性要求

为保证标签文件在多个版本之间的可用性，约定如下：

* 对于 **Label JSON 内部结构**：

  * 将来若新增字段：

    * 旧版本程序在遇到未知字段时，应忽略而非报错；
    * 新版本程序在读取旧标签时，若缺失某些新增字段，应使用默认值。
  * 必须字段：

    * `image_fixed`
    * `image_moving`
    * `rigid`
    * `matrix_3x3`
    * `tie_points`
  * 若上述字段缺失，则视为无效或损坏的标签。

* 对于 **文件命名规则**：

  * v0.x 系列版本保持当前 `<f>__<m>.json` 规则不变；
  * 如需更改，建议通过新子目录（如 `labels/v2/`）或 `meta.version` 字段来区分不同代际标签格式。

---

```
::contentReference[oaicite:0]{index=0}
```
