// 把前端构建产物（UI/dist）写进 exe 的资源清单。
//
// 为什么要生成：产物文件名带内容 hash（dist/assets/index-DWbN91t4.js），每次构建都可能变，
// 手写在 Resource.rc 里一次就得改一次。所以这里扫一遍 dist，直接重写 Resource.rc 里标记
// （; >>> dist / ; <<< dist）之间的那一块——资源名就是 URL 里的路径（assets/xxx.js），
// Page::onRequest 拿 URL 路径去 FindResource，两边对得上（见 DraftDepot/Page.cpp）。
//
// 为什么不单独生成一份 dist.rc 再让 Resource.rc include 它：rc.exe 不追踪被 include 文件的
// 改动，MSBuild 就认为资源是最新的、不重编，于是改了前端却把旧产物编进 exe（页面直接
// "app.localhost 拒绝连接"）。直接改 Resource.rc，它的 mtime 会变，资源必定重编。
//
// 跑法：npm run build 会自动带上它（见 package.json）；也可以在编译 exe 前单独跑一次。

import { readdirSync, readFileSync, writeFileSync, existsSync, statSync } from "node:fs";
import { join, relative } from "node:path";
import { fileURLToPath } from "node:url";

const uiDir = fileURLToPath(new URL("..", import.meta.url)); // 本脚本在 UI/scripts 下
const distDir = join(uiDir, "dist");
const rcFile = fileURLToPath(new URL("../../KnowNode/Resource.rc", import.meta.url));

/** Resource.rc 里由本脚本重写的那一块的首尾标记 */
const BEGIN = "; >>> dist";
const END = "; <<< dist";

/** 只嵌这些：.map 是源码映射，exe 里用不上，白占体积 */
const INCLUDED = /\.(html|js|mjs|css|json|png|jpg|jpeg|gif|svg|ico|woff2?|ttf|eot|wasm)$/i;

function walk(dir) {
  const files = [];
  for (const name of readdirSync(dir)) {
    const full = join(dir, name);
    if (statSync(full).isDirectory()) files.push(...walk(full));
    else files.push(full);
  }
  return files;
}

let body = "";

if (!existsSync(distDir) || !existsSync(join(distDir, "index.html"))) {
  // 还没构建过：留一份空清单，好让 exe 在 Debug（走 vite 开发服务器）下也能编译
  body = "; UI/dist 还没有，先给一份空清单。跑 npm run build 后会重新生成\n";
  console.warn("[gen-dist-rc] 没有找到 UI/dist/index.html，写了空清单：" + rcFile);
} else {
  const files = walk(distDir).filter((f) => INCLUDED.test(f)).sort();
  for (const file of files) {
    // 资源名 = 相对 dist 的路径（正斜杠），与 URL 里 https://app.localhost/ 后面那一截一致
    const name = relative(distDir, file).split("\\").join("/");
    // 文件路径相对 DraftDepot 目录（rc.exe 在那里编译 Resource.rc）。
    // 反斜杠在 rc 里是转义符（\a 会被吃掉），所以要写成双反斜杠
    const path = relative(fileURLToPath(new URL("../../DraftDepot", import.meta.url)), file)
      .split("\\")
      .join("\\\\");
    // 资源名不能加引号：加了引号 rc 就编不出能 FindResource 到的名字（名字里会带上引号本身），
    // 运行时一律 ERROR_RESOURCE_NAME_NOT_FOUND。裸名（像上面的 Msg.js）才查得到
    body += `${name} RCDATA "${path}"\n`;
  }
  console.log(`[gen-dist-rc] ${files.length} 个文件 → ${rcFile}`);
}

const rc = readFileSync(rcFile, "utf8");
const begin = rc.indexOf(BEGIN);
const end = rc.indexOf(END);
if (begin < 0 || end < 0) throw new Error(`Resource.rc 里找不到 dist 标记块：${BEGIN} / ${END}`);

writeFileSync(rcFile, rc.slice(0, begin + BEGIN.length) + "\n" + body + rc.slice(end));
