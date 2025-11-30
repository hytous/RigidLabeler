# Config 规范（config_spec.md）

本工程使用两个 YAML 配置文件：

- `config/backend.yaml`：后端（FastAPI）配置
- `config/app.yaml`：前端（Qt 客户端）配置

所有路径默认相对于工程根目录。

---

## 1. backend.yaml

后端服务及数据根目录配置。

### 1.1 示例

```yaml
server:
  host: "127.0.0.1"
  port: 8000

paths:
  data_root: "data"             # 数据总根目录（可选）
  labels_root: "data/labels"    # 标签文件根目录
  temp_root: "data/temp"        # 临时文件、预览图目录

logging:
  level: "INFO"                 # DEBUG / INFO / WARNING / ERROR
````

> 说明：
>
> * 后端对 `image_fixed` / `image_moving` 字段不做强制约束，可接受绝对路径或相对路径。
> * 若使用相对路径，可由业务逻辑自行选择是否与 `data_root` 组合。

### 1.2 字段说明

#### `server`

* `host` *(string)*
  FastAPI 监听地址，默认 `"127.0.0.1"`。

* `port` *(int)*
  FastAPI 监听端口，默认 `8000`。

#### `paths`

* `data_root` *(string, 可选)*
  数据根目录，可用于后续扩展（如拼接相对路径）。

* `labels_root` *(string)*
  标签保存目录。后端在启动时应确保该目录存在（不存在则创建）。

* `temp_root` *(string)*
  临时文件目录，用于存放 warp 预览图等。后端在启动时应确保该目录存在。

#### `logging`

* `level` *(string)*
  日志级别：`DEBUG` / `INFO` / `WARNING` / `ERROR`。

---

## 2. app.yaml

前端（Qt 客户端）配置，包括后端地址、默认路径与 UI 行为。

### 2.1 示例

```yaml
backend:
  base_url: "http://127.0.0.1:8000"

paths:
  default_images_root: "data/images"   # 第一次打开文件对话框时的初始目录
  default_labels_root: "data/labels"   # 前端浏览标签时的默认目录

ui:
  language: "zh-CN"                    # UI 语言（预留）
  theme: "light"                       # 主题：light / dark（预留）
  link_views_by_default: true          # 是否默认联动左右图像视图
  remember_last_dir: true              # 是否记住上一次选图目录

transform:
  allow_scale_default: false           # 计算变换时默认是否允许统一缩放
  min_points_required: 3               # 前端调用 /compute/rigid 前的点对数量下限
```

### 2.2 字段说明

#### `backend`

* `base_url` *(string)*
  Qt 客户端访问后端的基础 URL，例如 `"http://127.0.0.1:8000"`。
  `BackendClient` 通过该字段构造所有 API 请求地址。

#### `paths`

* `default_images_root` *(string)*
  第一次打开图像选择对话框时的初始目录。
  若启用了“记住上一次目录”功能，则仅在没有历史记录时使用该值。

* `default_labels_root` *(string)*
  前端浏览标签文件（如“标签浏览器”或打开标签文件对话框）时的默认目录。

#### `ui`

* `language` *(string)*
  UI 语言代码（如 `zh-CN`、`en-US`），当前版本可预留。

* `theme` *(string)*
  主题设置（`light` / `dark`），当前版本可预留。

* `link_views_by_default` *(bool)*
  是否在程序启动时就默认开启左右图像视图联动（缩放 / 平移同步）。

* `remember_last_dir` *(bool)*

  * `true`：前端使用 `QSettings`（或类似机制）记住用户最后一次成功选中图像文件所在的目录；
    下次打开文件对话框时，从该目录开始。
  * `false`：每次打开文件对话框都从 `default_images_root` 开始。

#### `transform`

* `allow_scale_default` *(bool)*
  “计算刚性变换”时的默认选项：

  * `true`：默认按相似变换（旋转 + 统一缩放 + 平移）估计；
  * `false`：默认按纯刚体变换（旋转 + 平移）估计。

* `min_points_required` *(int)*
  前端在调用 `/compute/rigid` 前进行的点数检查阈值。
  若当前点对数量小于该值，前端应给出提示并可阻止请求发送。
  后端仍须独立校验点数，避免依赖前端逻辑。

---

## 3. 加载策略与前端行为约定

### 3.1 前端路径记忆策略

1. 程序启动时：

   * 从 `QSettings`（或类似机制）读取 `last_image_dir`；
   * 若存在且目录有效，则作为当前图像选择对话框起始目录；
   * 否则，使用 `paths.default_images_root` 作为起始目录。

2. 用户通过文件对话框成功选择图像后：

   * 取所选文件所在目录，更新为新的 `last_image_dir`；
   * 写回 `QSettings`，供下次启动时使用。

3. 切换数据集：

   * 当用户在文件对话框中导航到另一数据集路径并选择图像后，
     `last_image_dir` 自动更新为该新路径，后续默认从此处开始。

### 3.2 后端配置加载

* 在 `rigidlabeler_backend/config.py` 中定义配置读取逻辑：

  * 加载 `backend.yaml`；
  * 若某些字段缺失，可使用合理默认值（如 `host="127.0.0.1"`、`port=8000` 等）。

### 3.3 前端配置加载

* 在 `AppConfig` 中负责：

  * 加载 `app.yaml`；
  * 将 `backend.base_url`、`paths.default_*`、`ui.*`、`transform.*` 等字段提供给其他模块使用。

```
::contentReference[oaicite:0]{index=0}
```
