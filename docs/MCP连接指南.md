# Pencil Dream MCP 智能体连接指南

Pencil Dream 内嵌了 MCP（Model Context Protocol）服务器，允许外部智能体（ZCode / Claude Code / Cursor 等支持 MCP 的客户端）通过对话直接操控软件，实现自动填色、自动画中割、批量帧管理等动画工作。

## 一、开启服务器

1. 打开 Pencil Dream → 菜单「窗口 → 首选项」（或工具栏设置入口）→ 左侧列表选择「MCP」页。
2. 勾选「启用 MCP 服务器」。服务器只监听本机回环地址（127.0.0.1），默认端口 **9528**。
3. 页面上可以看到自动生成的**访问令牌**（token），可复制或重新生成。

> 安全设计：所有请求必须携带令牌（`Authorization: Bearer`、`X-Pencil-Token` 请求头或 `?token=` 参数）；带浏览器 `Origin` 头的请求一律拒绝；`Host` 必须为本机回环。`GET http://127.0.0.1:9528/api/status` 免鉴权，可用来探活。

## 二、把 Pencil Dream 接入智能体客户端

首选项 MCP 页有「复制 MCP 客户端配置」按钮，一键得到下面这段 JSON（含实时端口与令牌），按所用客户端的说明添加即可：

```json
{
  "mcpServers": {
    "pencil-dream": {
      "type": "http",
      "url": "http://127.0.0.1:9528/mcp",
      "headers": {
        "Authorization": "Bearer <你的令牌>"
      }
    }
  }
}
```

- 协议端点：`POST /mcp`（也接受 `/jsonrpc`、`/`），JSON-RPC 2.0 + MCP（initialize / tools/list / tools/call / ping）。
- 「复制智能体连接提示词」按钮会把一份给智能体的中文使用说明放进剪贴板，粘到对话开头即可快速开始。

## 三、工具一览（26 个）

| 分组 | 工具 | 说明 |
|---|---|---|
| 感知 | `get_scene_status` | 图层树（行号/id/类型/关键帧分布）、当前帧、帧率、画布尺寸 |
| | `get_frame_image` | 渲染指定帧返回 PNG（智能体的"眼睛"；单层或合成） |
| | `get_palette` | 读取色卡 |
| 图层 | `create_layer` / `delete_layer` / `set_layer_visibility` / `select_layer` / `rename_layer` | |
| 帧 | `add_key_frame` / `duplicate_frame` / `delete_frame` / `scrub_to` | 全部走布局事务，单步撤销 |
| 绘制 | `draw_stroke` / `fill_region` / `clear_frame` | 坐标系=画布左上原点，与看图坐标一致 |
| 填色 | `set_colorize_options` / `request_colorize_update` | Krita 式智能填色引擎 |
| 中割 | `generate_inbetweens` | 距离场插值生成中间帧 |
| 项目 | `open_project` / `save_project` / `export_frame` / `export_movie` | 导出视频需配置 ffmpeg |
| 播放 | `play` / `stop` / `undo` / `redo` | |

图层引用规则：**数字 = 图层 id（优先）或时间轴行号（顶行 = 1）；字符串 = 图层名称**。引用失败时错误信息会附上当前图层清单，智能体可自行纠正。

## 四、两大典型工作流

### 自动填色

```
1. get_scene_status + get_frame_image        ← 看线稿，规划配色与区域种子点
2. create_layer(type="colorize", name="填色") ← 没有填色层时新建
3. draw_stroke(layer=填色层, frame=1,
     points=[[区域中心x,y]], color="#336699") ← 每个封闭区域点一颗种子
4. request_colorize_update(layer=填色层)      ← 触发计算，返回填色结果图
5. 看图复查 → 补种子 / set_colorize_options 调参 → 重算
```

填色线稿源自动取填色层上/下方最近的当前帧非空位图层。

### 自动画中割

```
1. get_frame_image(frame=原画A) + get_frame_image(frame=原画B)   ← 看两张原画
2. generate_inbetweens(layer=线稿层, frame_a=1, frame_b=5, count=3)
3. 逐帧 get_frame_image 复查 → 不满意的帧 delete_frame 后重画/重生成
```

距离场插值算法对平移/小幅形变稳定；大幅旋转会收缩（确定性算法上限）。

## 五、约束与提示

- 智能体所有修改类操作都进撤销栈：一次工具调用 ≈ 一步 Ctrl+Z。
- 工具调用串行执行；慢操作（填色计算、视频导出）期间新调用会收到"进行中"提示。
- 导出视频依赖 ffmpeg：在 首选项 → 文件 配置路径，或将 ffmpeg.exe 放到软件 plugins 目录。
- 二次开发参考：传输/协议层在 `app/src/mcp/mcpserver.cpp`，工具实现与 schema 在 `app/src/mcp/mcpdispatcher.cpp`，中割算法在 `core_lib/src/graphics/bitmap/inbetween.cpp`。
