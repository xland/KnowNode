# 11 · 平台、技术栈与工程结构

## 运行平台

- 基于 **WebView2** 开发。
- **只考虑兼容 Windows 操作系统**，不做跨平台（macOS / Linux 不在范围内）。

> 实现影响：Web 层只需兼容 Chromium 内核（WebView2 / Edge / Chrome），
> 无需为 Firefox / Safari 编写兼容分支（如 `scrollbar-width`、`@supports not selector(::-webkit-scrollbar)` 等）。

## 语言与代码构成

项目中同时存在两类代码：

| 类型 | 说明 | 位置 |
| --- | --- | --- |
| C++ | 宿主程序（WebView2 载体、原生能力、打包成 exe） | 主要在 `KnowNode/` 子目录 |
| 前端 | JS / CSS / HTML | 主要在 `UI/` 子目录 |

## 目录结构（现状）

```
KnowNode/
  Arch/        # 架构文档（本目录），唯一事实来源
  Doc/         # 设计资源
    logo.ico   # 应用图标（.ico），同时用于 exe 图标；用户已设计完成
  KnowNode/    # C++ 代码（宿主 / 原生层）—— **现行实现**：main.cpp / Window / Page / Db/ 五个表类
  UI/          # 前端代码
    public/    # 静态资源：logo.svg、iconfont.css、iconfont.woff2
    src/       # 前端源码（ts / scss / html）
    index.html
    vite.config.ts
  packages/    # 第三方包（WebView2 等），**用户已配置好，不要清理**
  x64/         # 构建产物（当前构建的输出目录），不是残留
  KnowNode.slnx
```

## 图标资源

- `Doc/logo.ico`：**唯一的应用图标文件**（.ico），同时作为 exe 图标与应用内图标使用。
  用户已设计完成，直接使用，不改动。
- `UI/public/logo.svg`：应用 logo 的 SVG 源文件（已按要求改为 `#1677ff` 蓝色圆角矩形底 + 白色图案）。

> 已确认：原 `UI/public/logo.ico` 与 `Doc/logo.ico` 是**同一个图标**，
> 用户已删除重复的那份，现只保留 `Doc/logo.ico` 与 `UI/public/logo.svg`。

## 已确认

- **C++ ↔ 前端通信**：`PostWebMessageAsJson` + `Page::onMsgReceived` 分发，详见 [21-ipc.md](./21-ipc.md)。
- **数据库文件位置**：沿用现有实现 `Env::getDataPath() / db.db`（数据目录由 `Env::initDataPath` 创建）。
- **前端产物加载**：Debug 连 vite 开发服务器 `http://localhost:5173`；Release 从 exe 资源应答
  `https://app.localhost/index.html`。
- **构建顺序与两个坑**（2026-10-06 编译验证过）：
  1. 先 `npm run build`——它会调 `UI/scripts/gen-dist-rc.mjs` 重写 `Resource.rc` 里的 dist 清单
     （产物文件名带 hash，手改跟不上；dist 清单过时就会 RC2135 编译不过）。
  2. 资源编译器**不继承** `ClCompile` 的 `/D_DEBUG`，所以 `Resource.rc` 的 `#ifndef _DEBUG`
     要靠 vcxproj 里单独的 `<ResourceCompile><PreprocessorDefinitions>_DEBUG;...` 才成立
     （已加在 Debug|x64）；否则 Debug 也去嵌 dist，dist 没构建就编译不过。
  Debug / Release | x64 均已编译通过。

## 待确认问题

- ~~`packages/`、`x64/`、`KnowNode/` 中哪些是老项目残留、需要清理或重写~~
  —— **已定（2026-10-08）**：`KnowNode/` 是**现行实现不是残留**（里面的「文章」相关残留已清理完）；
  `packages/` 是第三方依赖、不动；`x64/` 与 `KnowNode/x64/` 是构建产物。
