# SoftPal-Sakura ❖ 解封包工具

<div align="center">
  <img src="static/bg.png" width="400" alt="SoftPal-Sakura Background" />
</div>

> 面向 **SoftPal (Pal) 引擎** 的图形化解包 / 反汇编 / 重建 / 解密工具。

> 桜 可爱捏 ❖

**[English →](README.md)**

---

> ⚠️ **免责声明与警告 (Disclaimer)**  
> 本工具的开发与发布**仅供技术交流与个人学习研究使用**，严禁用于任何商业用途或侵权行为。  
> 通过本工具所提取出来的游戏资源（图像、音频、文本等资料），其全部版权与知识产权均归原厂商所有。  
> **请低调使用，切勿大肆宣扬或将本工具用于大规模盗版、汉化商业牟利等行为**  
> 使用者因使用本工具而产生的一切后果与责任，由使用者自行承担。

---

## 1. 如何使用

只要两件东西：

1. Windows 系统
2. 游戏为 SoftPal / Pal 引擎

> 💡 **怎么判断是不是 SoftPal 引擎**：游戏目录里有 `dll/Pal.dll` 即是。

至于「工具」本身的运行文件，只有这两个：

- `SoftPal-Sakura.exe`（主程序，双击运行）
- `SoftPal-Sakura.dll`（核心库，必须和 exe 放在同一个目录里）

> ⚠️ **重要**：这两个文件 **必须放在一起**。少一个或分开放，程序启动会失败。

---

## 2. 放在哪里

推荐做法：把 `SoftPal-Sakura.exe` 和 `SoftPal-Sakura.dll` **直接复制到游戏根目录**，也就是和 `data.pac`、`system.pac`、`dll\` 等数据同一层。这样工具会自动识别所有默认路径，不用自己填。

示意：

```
<游戏目录>\
├── data.pac
├── system.pac
├── bgm.pac
├── ev.pac
├── ...
├── dll\
│   └── Pal.dll
├── SoftPal-Sakura.exe        ← 放这里
└── SoftPal-Sakura.dll        ← 放这里
```

放好后双击 `SoftPal-Sakura.exe` 即可。

---

## 3. 界面长什么样

打开之后是一个奶白底的小窗口，上面有两个页签：

| 页签 | 作用 |
| --- | --- |
| **解包 Extract** | 拖入 `.pac` 归档进行解包（内部 `.PGD` 自动转 `.PNG`）。 |
| **封包 Pack** | 把 `SCRIPT.SRC` / `TEXT.DAT` 反汇编成 JSON、由 JSON 重建、或对老版本进行 PAL 解密。 |

窗口最下方有：

- **状态条**：会告诉你现在在干嘛，成功还是失败。
- **进度条**：长任务会实时走动。
- **当前步骤**：更详细的一行说明。
- **打开结果** 按钮：任务完成后，直接跳到输出位置。
- **重置状态** 按钮：清掉当前状态，回到待机界面准备下一次任务。

---

## 4. 解包

1. 打开 `SoftPal-Sakura.exe`。
2. 停留在 **解包 Extract** 页签。
3. 从资源管理器里，**把 `.pac` 拖进那块大块的拖放区域**：
   - 拖 `data.pac` → 解出 `SCRIPT.SRC`、`TEXT.DAT` 等到 `unpack\data\`
   - 拖 `ev.pac` / `bk.pac` / `st.pac` → 图片归档；里面的 `.PGD` 会被自动解码成 `.PNG`
   - 拖 `bgm.pac` / `se.pac` / `voice.pac` → 音频归档
4. 等进度条走完，状态条显示「解包完成」。
5. 点右下角 **打开结果**，就能直接看到导出的文件。

可以一次性拖多个文件进去，它会挨个处理。输出统一放在 `unpack\<归档名>\`。

> 💡 PGD 叠图（依赖底图的 `.PGD`）会被自动按顺序处理 —— 核心层先处理底图，再把叠图合成出最终 PNG。

---

## 5. 翻译脚本文本

整体流程：**反汇编 → 修改 JSON → 重建**。

1. 先按上面的方法把含 `SCRIPT.SRC` / `TEXT.DAT` 的归档（一般是 `data.pac`）拖进来解包。文件会出现在 `unpack\data\`。
2. 切到 **封包 Pack** 页签。
3. 在 **脚本反汇编 → JSON** 卡片：
   - 「SCRIPT.SRC」选 `unpack\data\SCRIPT.SRC`（如果同目录下能找到 `TEXT.DAT` 与默认 JSON 路径，工具会自动填好）。
   - 「TEXT.DAT」选 `unpack\data\TEXT.DAT`。
   - 「输出 JSON」默认 `unpack\data\script_export.json`。
   - 点 **导出 JSON**。
4. 打开生成的 JSON。每条对话格式如下，**只填 `Translate` 字段，其它字段一律不要动**：

   ```json
   {
       "Text": {
           "Original": "「やーっと起きた」",
           "Translate": "「やーっと起きた」",
           "TextOffset": 7638
       },
       "Name": {
           "Original": "少女",
           "Translate": "少女",
           "TextOffset": 7661
       },
       "ScriptOffset": 444044
   }
   ```

5. 回到工具，在 **脚本重建** 卡片：
   - 「翻译 JSON」选你刚改完的那份。
   - 「输出目录」选个想写出新文件的地方（默认就在 JSON 旁边）。
   - 「编码」中文选 `gbk`，要保留原日文就选 `shift_jis`。
   - 点 **重建**。
6. 工具会在输出目录生成新的 `SCRIPT.SRC` + `TEXT.DAT`。直接放回游戏的 `data\` 目录即可 —— **不需要重新封 .pac、不需要重新加密**，引擎会优先读散落的明文文件。

> 💡 当前支持的字节码列表见文末。**未识别到的字节码所引用的文本会原地保留只做编码转换**，不会写入翻译，避免脚本解析不全导致游戏读到错误文本。

---

## 6. PAL 解密（仅用于较老版本）

新版 SoftPal 多为明文，`SCRIPT.SRC` / `TEXT.DAT` 直接可读。部分老版本会在外层包一层 ROL+XOR 加密，这种情况下反汇编会失败 —— 必须先解密。

1. 在 **封包 Pack** 页签底部找到 **PAL 解密 (ROL + XOR)** 卡片。
2. 「输入」选加密的文件（常见后缀是 `.enc`，也可能就是被加密的 `SCRIPT.SRC`）。
3. 「输出」默认 `<输入>.dec`。
4. 点 **解密**。
5. 用解密后的文件作为第 5 节反汇编步骤的输入。

> 💡 如果 `SCRIPT.SRC` 直接反汇编就成功，那你拿到的就是新版明文，**不需要这一步**。

---

## 7. 常见问题

**Q：双击 exe 直接闪退？**  
A：十有八九是 `SoftPal-Sakura.dll` 没放在同一目录。检查一下。

**Q：拖文件进去没反应？**  
A：解包页签只认 `.pac` 归档。其他类型请改用封包页签的对应卡片。

**Q：反汇编出来全是乱码？**  
A：你的 `SCRIPT.SRC` 是加密版本。先做第 6 节的 **PAL 解密**，再把解密后的文件拿来反汇编。

**Q：把重建的文件复制回去后游戏崩溃？**  
A：要么翻译里有目标编码不支持的字符，要么单条文本超过引擎演出限制（通常约 128 字节 / 调用）。看「当前步骤」里的报错偏移，或者临时把原版文件还回去定位。

**Q：想重新开始？**  
A：随时点右下角的 **重置状态** 按钮即可。

**Q：必须重新封 `.pac` 吗？**  
A：**不需要**。引擎读 `data\` 下的散落文件优先级高于归档，所以重建得到的 `SCRIPT.SRC` / `TEXT.DAT` 直接放进 `data\` 就生效。

---

## 8. 目录速查

| 路径 | 存什么 |
| --- | --- |
| `unpack\<归档名>\` | 单个归档的解包根目录（每拖一个 `.pac` 生成一个） |
| `unpack\<归档名>\SCRIPT.SRC` | 脚本字节码（一般在 `data.pac` 里） |
| `unpack\<归档名>\TEXT.DAT` | 脚本引用的文本表 |
| `unpack\<归档名>\*.PNG` | 解包过程中由 `.PGD` 自动解码出的 PNG |
| `unpack\data\script_export.json` | 反汇编默认输出 —— 翻译就是改这个 |
| `<输出目录>\SCRIPT.SRC` + `TEXT.DAT` | 重建产物，放回游戏 `data\` 目录即可生效 |
| `<输入>.dec` | PAL 解密默认输出 |

---

## 9. 开发者 · 从源码构建

只想自己编译一份来玩 / 改的话，流程很简单：

- 装好 **Visual Studio 2022 或 2026**（勾选「使用 C++ 的桌面开发」+「C++/CLI 支持」两个组件）。
- **CMake ≥ 3.20**（VS 自带的就够用）。

命令行：

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target softpal_ui
```

新版工具链：

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release --target softpal_ui
```

产物位于 `build\bin\Release\`：

- `SoftPal-Sakura.exe` —— GUI 主程序（C++/CLI，依赖 .NET Framework 4.x，Win10 / 11 自带）
- `SoftPal-Sakura.dll` —— 核心库（原生 C++，导出 `sp_*` 系列 API）

### 工程结构

```
SoftPal-Sakura/
├── CMakeLists.txt              顶层 CMake 脚本
├── src/
│   ├── core/                   原生 C++ 核心（DLL）
│   │   ├── core_exports.{h,cpp}    对外 C ABI：sp_pac_*, sp_script_*, sp_pal_*, sp_dir_convert_pgd
│   │   ├── pac.cpp                 PAC 归档打开 / 解包
│   │   ├── pgd.cpp / pgd_decode.cpp PGD 图像解码（含 PGD3 叠图）→ PNG
│   │   ├── pal_decrypt.cpp         旧版 SCRIPT.SRC / TEXT.DAT 的 ROL + XOR 解密
│   │   ├── script.cpp              SCRIPT.SRC / TEXT.DAT 反汇编 + 重建
│   │   ├── json_mini.{h,cpp}       内嵌的极简 JSON 读 / 写
│   │   ├── util.cpp                公用工具：编码、路径、IO
│   │   └── stb_image_write.h       单文件头的 PNG 写入器
│   └── ui/                     GUI EXE（C++/CLI WinForms）
│       ├── main.cpp                wWinMain 入口
│       ├── MainForm.h              定制化 WinForms 主窗体（圆角卡片 / 软色描边）
│       └── app.rc                  内嵌图标 (`icon.ico`) 与背景 (`bg.png`)
├── static/                     窗口背景与图标素材
└── README.md / README.zh-CN.md
```

### 当前支持解析的字节码

1. **Text Show**：`Hi 00 02 Lo 00 02`、`00 0F`、`00 10`、`00 11`、`00 12`、`00 13`、`00 14`
2. **Select 选项**：`Hi 00 06 Lo 00 02`

可编辑内容：对话文本、对话角色名、选项文本。引擎单次最大演出文本约 **128 字节**，在此限制内翻译可以是任意长度，重建器会自动重排文本表偏移。

### 依赖与说明

- 源码编码默认按 **Shift_JIS** 解析；重建时默认目标编码为 **GBK**。如果要发布中文版本，通常还需要替换游戏字体并修改引擎里的字符范围校验，这两件事在工具范围之外。
- **未实现 PAC 重打包**：因为引擎读 `data\` 散落文件的优先级高于归档，翻译流程不需要重新封包。
- 不同版本的引擎实现细节会有差异，本项目不保证兼容所有版本。如果某个游戏反汇编失败，欢迎附上小样本反馈。

---

Enjoy your patching! ❖  若遇到 bug 欢迎反馈~
