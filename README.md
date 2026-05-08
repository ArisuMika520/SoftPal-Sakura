# SoftPal-Sakura ❖

<div align="center">
  <img src="static/bg.png" width="400" alt="SoftPal-Sakura Background" />
</div>

> A graphical unpack / disassemble / rebuild / decrypt tool for games built on the **SoftPal (Pal) engine**.

> Sakura is so cute ❖

**[中文文档 →](README.zh-CN.md)**

---

> ⚠️ **Disclaimer & Warning**  
> This tool is created for **educational purposes and technical research only**. Any commercial use or copyright infringement is strictly prohibited.  
> All extracted assets (images, audio, text, etc.) are the intellectual property of their respective original creators and copyright holders.  
> **Please use this tool discreetly. Do not spread it widely or use it for mass piracy distribution to avoid potential legal issues, including DMCA takedowns.** The authors of this tool assume no responsibility or liability for any consequences arising from its use.

---

## 1. Requirements

Two things are all you need:

1. Windows
2. A game that runs on the SoftPal / Pal engine

> 💡 **How to recognise a SoftPal game**: the install folder contains `dll/Pal.dll`. 

The tool itself ships as two files:

- `SoftPal-Sakura.exe` — main program (double-click to launch)
- `SoftPal-Sakura.dll` — core library (must be in the **same folder** as the exe)

> ⚠️ **Important**: both files **must sit together**. If either is missing or placed elsewhere, the program will fail to start.

---

## 2. Where to Put the Files

The recommended approach is to copy `SoftPal-Sakura.exe` and `SoftPal-Sakura.dll` **directly into the game's root directory** — the same folder that contains `data.pac`, `system.pac`, the `dll\` subfolder, etc. The tool will then pick up game files with no manual path configuration.

Example layout:

```
<Game>\
├── data.pac
├── system.pac
├── bgm.pac
├── ev.pac
├── ...
├── dll\
│   └── Pal.dll
├── SoftPal-Sakura.exe        ← here
└── SoftPal-Sakura.dll        ← here
```

Double-click `SoftPal-Sakura.exe` to start.

---

## 3. Interface Overview

The window has two tabs:

| Tab | Purpose |
| --- | --- |
| **Extract** | Drop `.pac` archives to unpack their contents (auto-converts `.PGD` → `.PNG`). |
| **Pack** | Disassemble `SCRIPT.SRC` / `TEXT.DAT` to JSON, rebuild from JSON, or decrypt PAL-encrypted files. |

At the bottom of the window you will find:

- **Status bar** — tells you what the tool is doing and whether it succeeded.
- **Progress bar** — moves in real time for long tasks.
- **Current step** — a more detailed single-line description.
- **Open Result** button — jumps to the output folder when a task finishes.
- **Reset State** button — clears the current status and gets you ready for the next task.

---

## 4. Extracting

1. Open `SoftPal-Sakura.exe`.
2. Stay on the **Extract** tab.
3. Drag any `.pac` archive onto the large drop area:
   - `data.pac` → unpacks `SCRIPT.SRC`, `TEXT.DAT`, and other resources to `unpack\data\`
   - `ev.pac` / `bk.pac` / `st.pac` → image archives; any `.PGD` inside is automatically decoded to `.PNG`
   - `bgm.pac` / `se.pac` / `voice.pac` → audio archives
4. Wait for the progress bar to finish and the status bar to show "Extraction complete".
5. Click **Open Result** in the bottom-right to browse the output directly.

Multiple files can be dropped at once; they will be processed one after another. Output always goes under `unpack\<archive-name>\`.

> 💡 PGD overlays (`.PGD` images that depend on a base image) are handled automatically — the core orders base files first, then composites the overlays into the final PNG.

---

## 5. Translating Script Text

The end-to-end flow is **Disassemble → edit JSON → Rebuild**.

1. First extract the archive that holds `SCRIPT.SRC` and `TEXT.DAT` (typically `data.pac`). They will appear in `unpack\data\`.
2. Switch to the **Pack** tab.
3. In the **Script Disassemble → JSON** card:
   - "SCRIPT.SRC" — pick `unpack\data\SCRIPT.SRC` (the tool will auto-fill `TEXT.DAT` and the JSON output if it can find them).
   - "TEXT.DAT" — pick `unpack\data\TEXT.DAT`.
   - "Output JSON" — defaults to `unpack\data\script_export.json`.
   - Click **Export JSON**.
4. Open the generated JSON. Each entry looks like this; only fill in `Translate`, leave everything else alone:

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

5. Back in the tool, in the **Script Rebuild** card:
   - "Translation JSON" — point at the JSON you just edited.
   - "Output Directory" — choose where to write the rebuilt files (defaults next to the JSON).
   - "Encoding" — `gbk` for Chinese, `shift_jis` to keep the original Japanese encoding.
   - Click **Rebuild**.
6. The tool produces fresh `SCRIPT.SRC` + `TEXT.DAT` in the output directory. Drop them straight into the game's `data\` folder — no need to repack the `.pac` and no need to re-encrypt; the engine reads loose files first.

> 💡 The supported in-engine bytecodes are listed at the bottom of this document. Lines whose opcodes are not yet handled are passed through unchanged (encoding-converted only) so the game never reads broken text.

---

## 6. PAL Decrypt (older builds only)

Newer SoftPal builds ship `SCRIPT.SRC` / `TEXT.DAT` in plaintext. Some older builds wrap them in a ROL+XOR layer — those files won't disassemble cleanly until you decrypt them first.

1. In the **Pack** tab, locate the **PAL Decrypt (ROL + XOR)** card at the bottom.
2. "Input" — pick the encrypted file (often named with a `.enc` suffix, or simply the encrypted `SCRIPT.SRC`).
3. "Output" — defaults to `<input>.dec`.
4. Click **Decrypt**.
5. Use the decrypted file as the input for the disassembly step in section 5.

> 💡 If `SCRIPT.SRC` already disassembles cleanly, you don't need this step — your build is the newer plaintext variant.

---

## 7. FAQ

**Q: The exe closes immediately after double-clicking.**  
A: `SoftPal-Sakura.dll` is almost certainly missing from the same directory. Check the placement.

**Q: Nothing happens after dropping a file.**  
A: Only `.pac` archives are accepted on the Extract drop area. For other formats use the Pack tab instead.

**Q: Disassemble fails with garbled output.**  
A: Your `SCRIPT.SRC` is encrypted. Run **PAL Decrypt** first (section 6), then feed the decrypted file into Disassemble.

**Q: The game crashes after I copy the rebuilt files.**  
A: Either a translated line contains a character outside the chosen encoding, or it is too long for an in-engine text field (engine limit ~128 bytes per text rendering call). Check **Current Step** for the failing offset, or temporarily restore the original files to confirm.

**Q: I want to start fresh.**  
A: Click **Reset State** in the bottom-right at any time.

**Q: Will repacked text show up if I leave the original `.pac` in place?**  
A: Yes — the engine reads loose files in `data\` before consulting the archive. You do **not** need to rebuild the `.pac` itself.

---

## 8. Directory Reference

| Path | Contents |
| --- | --- |
| `unpack\<archive>\` | Per-archive extraction root (one folder per `.pac` you drop) |
| `unpack\<archive>\SCRIPT.SRC` | Script bytecode (in `data.pac`) |
| `unpack\<archive>\TEXT.DAT` | Text table referenced by the script |
| `unpack\<archive>\*.PNG` | Images auto-decoded from `.PGD` during extraction |
| `unpack\data\script_export.json` | Default Disassemble output — the file you edit for translation |
| `<output dir>\SCRIPT.SRC` + `TEXT.DAT` | Rebuild output — drop into the game's `data\` folder |
| `<input>.dec` | Default PAL Decrypt output |

---

## 9. Building from Source

Requirements:

- **Visual Studio 2022 or 2026** with the **"Desktop development with C++"** and **"C++/CLI support"** workloads installed.
- **CMake ≥ 3.20** (the version that ships inside Visual Studio is fine).

From the command line:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target softpal_ui
```

Or, on a newer toolchain:

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release --target softpal_ui
```

Output is placed in `build\bin\Release\`:

- `SoftPal-Sakura.exe` — GUI host (C++/CLI, requires .NET Framework 4.x — built into Windows 10/11)
- `SoftPal-Sakura.dll` — core library (native C++, exports the `sp_*` API family)

### Project Structure

```
SoftPal-Sakura/
├── CMakeLists.txt              Top-level CMake script
├── src/
│   ├── core/                   Native C++ core (DLL)
│   │   ├── core_exports.{h,cpp}    Public C ABI: sp_pac_*, sp_script_*, sp_pal_*, sp_dir_convert_pgd
│   │   ├── pac.cpp                 PAC archive open / extract
│   │   ├── pgd.cpp / pgd_decode.cpp PGD image decode (incl. PGD3 overlays) → PNG
│   │   ├── pal_decrypt.cpp         ROL + XOR decryption for legacy SCRIPT.SRC / TEXT.DAT
│   │   ├── script.cpp              SCRIPT.SRC / TEXT.DAT disassemble + rebuild
│   │   ├── json_mini.{h,cpp}       Minimal embedded JSON reader/writer
│   │   ├── util.cpp                Shared helpers (encoding, paths, IO)
│   │   └── stb_image_write.h       Single-header PNG writer
│   └── ui/                     GUI EXE (C++/CLI WinForms)
│       ├── main.cpp                wWinMain entry point
│       ├── MainForm.h              Custom WinForms main window with rounded cards / soft borders
│       └── app.rc                  Embedded icon (`icon.ico`) and background (`bg.png`)
├── static/                     Window background, icon assets
└── README.md / README.zh-CN.md
```

### Currently Supported Script Opcodes

1. **Text Show**: `Hi 00 02 Lo 00 02`, `00 0F`, `00 10`, `00 11`, `00 12`, `00 13`, `00 14`
2. **Choice / Select**: `Hi 00 06 Lo 00 02`

Editable content via the JSON workflow: dialogue text, speaker names, choice text. The in-engine maximum rendered text length is **128 bytes per call**, but within that limit translated text may be of any length; the rebuilder repositions text-table entries automatically.

### Notes

- Source encoding is assumed to be **Shift_JIS**. The default rebuild target encoding is **GBK**; if you ship Chinese, you will likely also need to swap the in-game font and adjust the engine's character-range validator. Both edits live outside this tool.
- PAC repacking is **not** implemented — the engine loads loose files in `data\` ahead of the archive, so it isn't required for translation work.
- Engine versions vary; this project doesn't claim to support every shipping build. If a specific game refuses to disassemble, please open an issue with a small reproducer.

---

Enjoy your patching! ❖
