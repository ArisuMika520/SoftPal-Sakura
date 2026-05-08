// 脚本反汇编 + 重建
// 含其中针对 Select 误报的修正
#include "core_exports.h"
#include "internal.h"
#include "json_mini.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <map>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

namespace sp::script
{

    struct TextEntry
    {
        uint32_t offset = 0; // 原始 TEXT.DAT 中条目起始偏移（即 4 字节索引头的位置）
        uint8_t index[4] = {};
        std::vector<uint8_t> text; // 索引之后到 NUL 终止符之间的字节
        std::string text_utf8;
        bool is_modified = false;
        uint32_t new_offset = 0;
    };

    class TextPack
    {
    public:
        bool load(const std::vector<uint8_t> &bytes)
        {
            if (bytes.size() < 16)
            {
                set_last_error("TEXT.DAT too small (header < 16).");
                return false;
            }
            bytes_ = bytes;
            entries_.clear();
            offset_to_id_.clear();

            size_t offset = 16;
            size_t i = 0;
            while (offset < bytes_.size())
            {
                size_t scan_from = offset + 4;
                size_t text_end = scan_from;
                while (text_end < bytes_.size() && bytes_[text_end] != 0)
                    ++text_end;

                TextEntry e;
                e.offset = uint32_t(offset);
                if (offset + 4 <= bytes_.size())
                {
                    std::memcpy(e.index, bytes_.data() + offset, 4);
                }
                size_t body_start = offset + 4;
                size_t body_end = (text_end == bytes_.size()) ? bytes_.size() : text_end;
                if (body_end > body_start)
                {
                    e.text.assign(bytes_.begin() + body_start, bytes_.begin() + body_end);
                }
                std::string raw(reinterpret_cast<const char *>(e.text.data()), e.text.size());
                e.text_utf8 = sp::decode_to_utf8(raw, sp::CP_SHIFT_JIS);

                offset_to_id_[e.offset] = i++;
                entries_.push_back(std::move(e));

                // 步长 = 索引(4) + 文本 + 1 字节 NUL；若文件以非 NUL 结尾则与 Python 一致直接退出
                size_t item_len = 4 + (body_end - body_start);
                offset += item_len + 1;
                if (text_end == bytes_.size())
                    break;
            }
            return true;
        }

        const std::vector<TextEntry> &entries() const { return entries_; }
        std::vector<TextEntry> &entries() { return entries_; }
        const std::vector<uint8_t> &bytes() const { return bytes_; }

        int find_id_by_offset(uint32_t offset) const
        {
            auto it = offset_to_id_.find(offset);
            return it == offset_to_id_.end() ? -1 : int(it->second);
        }
        bool has_offset(uint32_t offset) const { return offset_to_id_.count(offset) != 0; }

        // 修改条目并记入 modified_indices_，以便 rebuild 时把它再追加到 TEXT.DAT 末尾
        bool modify(uint32_t offset, const std::string &new_utf8, unsigned target_cp)
        {
            int idx = find_id_by_offset(offset);
            if (idx < 0)
                return false;
            TextEntry &e = entries_[size_t(idx)];
            std::string encoded = sp::encode_from_utf8(new_utf8, target_cp);
            // 单独的 '?' 在引擎中可能被解读为转义引导字节，重复一次以转义自身
            std::string replaced;
            replaced.reserve(encoded.size());
            for (char c : encoded)
            {
                if (c == '?')
                    replaced.append("??");
                else
                    replaced.push_back(c);
            }
            e.text.assign(replaced.begin(), replaced.end());
            e.is_modified = true;
            modified_indices_.push_back(size_t(idx));
            return true;
        }

        // 输出布局：
        //   16 字节头（首字节清零）+ 全部条目（按原顺序，未改的转目标编码）+ 修改过的条目再追加一次
        // 与 PalTextPack.rebuild() 字节级一致；offsets 会在两遍中各自记录一次 new_offset
        void rebuild_into(std::vector<uint8_t> &out, unsigned target_cp)
        {
            out.clear();
            out.reserve(bytes_.size());
            out.push_back(0);
            for (size_t i = 1; i < 16 && i < bytes_.size(); ++i)
                out.push_back(bytes_[i]);

            uint32_t new_offset = entries_.empty() ? 16u : entries_.front().offset;

            for (auto &e : entries_)
            {
                if (!e.is_modified)
                {
                    std::string raw(reinterpret_cast<const char *>(e.text.data()), e.text.size());
                    std::string utf8 = sp::decode_to_utf8(raw, sp::CP_SHIFT_JIS);
                    std::string enc = sp::encode_from_utf8(utf8, target_cp);
                    std::string replaced;
                    replaced.reserve(enc.size());
                    for (char c : enc)
                    {
                        if (c == '?')
                            replaced.append("??");
                        else
                            replaced.push_back(c);
                    }
                    e.text.assign(replaced.begin(), replaced.end());
                }

                std::vector<uint8_t> compiled = compile_entry(e);
                e.new_offset = new_offset;
                out.insert(out.end(), compiled.begin(), compiled.end());
                new_offset += uint32_t(compiled.size());
            }

            // 修改过的条目再追加一次（脚本会被回填指向追加位置的新偏移）
            for (size_t idx : modified_indices_)
            {
                TextEntry &e = entries_[idx];
                std::vector<uint8_t> compiled = compile_entry(e);
                e.new_offset = new_offset;
                out.insert(out.end(), compiled.begin(), compiled.end());
                new_offset += uint32_t(compiled.size());
            }
        }

        const std::vector<size_t> &modified_indices() const { return modified_indices_; }

    private:
        static std::vector<uint8_t> compile_entry(const TextEntry &e)
        {
            // 引擎不接受连续的全角空格 0xA1 0xA1，按 2 字节对齐扫描去掉，再补 NUL 终止
            std::vector<uint8_t> combined;
            combined.reserve(4 + e.text.size());
            combined.insert(combined.end(), e.index, e.index + 4);
            combined.insert(combined.end(), e.text.begin(), e.text.end());

            std::vector<uint8_t> stripped;
            stripped.reserve(combined.size() + 1);
            for (size_t i = 0; i < combined.size(); i += 2)
            {
                if (i + 1 < combined.size())
                {
                    uint8_t a = combined[i], b = combined[i + 1];
                    if (a == 0xA1 && b == 0xA1)
                        continue;
                    stripped.push_back(a);
                    stripped.push_back(b);
                }
                else
                {
                    // 末尾如有单字节落单则保留：单字节绝对不等于 0xA1 0xA1
                    stripped.push_back(combined[i]);
                }
            }
            stripped.push_back(0);
            return stripped;
        }

        std::vector<uint8_t> bytes_;
        std::vector<TextEntry> entries_;
        std::unordered_map<uint32_t, size_t> offset_to_id_;
        std::vector<size_t> modified_indices_;
    };

    enum class OpKind
    {
        TextShow,
        Select
    };

    struct ScriptOp
    {
        OpKind kind;
        uint32_t offset = 0; // SCRIPT.SRC 内的字节偏移
        uint32_t text_offset = 0;
        uint32_t name_offset = 0;
        bool has_name = false;
    };

    struct Disassembler
    {
        std::vector<uint8_t> script_bytes;
        TextPack text_pack;
        std::vector<ScriptOp> ops;
        std::unordered_map<uint32_t, size_t> op_offset_to_id;

        bool load(const fs::path &script_path, const fs::path &text_path)
        {
            std::vector<uint8_t> tb;
            if (!sp::read_file(script_path, script_bytes))
                return false;
            if (!sp::read_file(text_path, tb))
                return false;
            if (!text_pack.load(tb))
                return false;
            scan_ops();
            return true;
        }

        void scan_ops()
        {
            ops.clear();
            op_offset_to_id.clear();
            const size_t n = script_bytes.size();
            if (n < 8)
                return;

            static const std::array<std::array<uint8_t, 2>, 7> dialog_lo = {{
                {{0x02, 0x00}},
                {{0x0F, 0x00}},
                {{0x10, 0x00}},
                {{0x11, 0x00}},
                {{0x12, 0x00}},
                {{0x13, 0x00}},
                {{0x14, 0x00}},
            }};

            auto is_dialog_lo = [&](uint8_t a, uint8_t b)
            {
                for (auto &m : dialog_lo)
                    if (m[0] == a && m[1] == b)
                        return true;
                return false;
            };

            for (size_t i = 0; i + 8 <= n; i += 4)
            {
                const uint8_t *p = script_bytes.data() + i;
                // 文本 / 选项指令的对齐标记：17 00 01 00
                if (!(p[0] == 0x17 && p[1] == 0x00 && p[2] == 0x01 && p[3] == 0x00))
                    continue;
                uint8_t lo0 = p[4], lo1 = p[5];
                uint8_t hi0 = p[6], hi1 = p[7];
                if (hi0 == 0x02 && hi1 == 0x00 && is_dialog_lo(lo0, lo1))
                {
                    if (i < 24)
                        continue;
                    ScriptOp op;
                    op.kind = OpKind::TextShow;
                    op.offset = uint32_t(i - 24);
                    op.text_offset = sp::read_u32_le(script_bytes.data() + (i - 24) + 4);
                    op.name_offset = sp::read_u32_le(script_bytes.data() + (i - 24) + 12);
                    op.has_name = (op.name_offset != 0x0FFFFFFFu);
                    op_offset_to_id[op.offset] = ops.size();
                    ops.push_back(op);
                }
                else if (hi0 == 0x06 && hi1 == 0x00 && lo0 == 0x02 && lo1 == 0x00)
                {
                    if (i < 8)
                        continue;
                    ScriptOp op;
                    op.kind = OpKind::Select;
                    op.offset = uint32_t(i - 8);
                    op.text_offset = sp::read_u32_le(script_bytes.data() + (i - 8) + 4);
                    op.has_name = false;
                    // 误报过滤：选项指令的字节模式与某些跳转匹配，靠 text_offset 是否在 TEXT.DAT 内来甄别
                    if (!text_pack.has_offset(op.text_offset))
                        continue;
                    op_offset_to_id[op.offset] = ops.size();
                    ops.push_back(op);
                }
            }
        }

        bool export_json(const fs::path &json_path)
        {
            sp::json::Array arr;
            arr.reserve(ops.size());
            for (const auto &op : ops)
            {
                sp::json::Object item;

                int tid = text_pack.find_id_by_offset(op.text_offset);
                std::string text_utf8 = (tid >= 0) ? text_pack.entries()[size_t(tid)].text_utf8
                                                   : std::string{};

                sp::json::Object text_obj;
                text_obj.emplace_back("Original", sp::json::Value(text_utf8));
                text_obj.emplace_back("Translate", sp::json::Value(text_utf8));
                text_obj.emplace_back("TextOffset", sp::json::Value(int64_t(op.text_offset)));

                item.emplace_back("Text", sp::json::Value(std::move(text_obj)));

                if (op.has_name)
                {
                    int nid = text_pack.find_id_by_offset(op.name_offset);
                    std::string name_utf8 = (nid >= 0) ? text_pack.entries()[size_t(nid)].text_utf8
                                                       : std::string{};
                    sp::json::Object name_obj;
                    name_obj.emplace_back("Original", sp::json::Value(name_utf8));
                    name_obj.emplace_back("Translate", sp::json::Value(name_utf8));
                    name_obj.emplace_back("TextOffset", sp::json::Value(int64_t(op.name_offset)));
                    item.emplace_back("Name", sp::json::Value(std::move(name_obj)));
                }
                else
                {
                    item.emplace_back("Name", sp::json::Value(nullptr));
                }
                item.emplace_back("ScriptOffset", sp::json::Value(int64_t(op.offset)));
                arr.push_back(sp::json::Value(std::move(item)));
            }
            std::string text = sp::json::dump(sp::json::Value(std::move(arr)), 4);
            return sp::write_file(json_path, text.data(), text.size());
        }

        bool rebuild(const fs::path &json_path,
                     const fs::path &out_script_path,
                     const fs::path &out_text_path,
                     unsigned target_cp)
        {
            std::vector<uint8_t> raw;
            if (!sp::read_file(json_path, raw))
                return false;
            std::string utf8(reinterpret_cast<const char *>(raw.data()), raw.size());
            sp::json::Value root;
            std::string err;
            if (!sp::json::parse(utf8, root, err))
            {
                sp::set_last_error_fmt("JSON parse error: %s", err.c_str());
                return false;
            }
            if (!root.is_array())
            {
                sp::set_last_error("JSON root is not an array.");
                return false;
            }
            const auto &arr = root.as_array();

            // 先 Name 后 Text 的顺序与 Python 对齐，决定追加段在 TEXT.DAT 末尾的写入顺序
            for (const auto &v : arr)
            {
                const auto *name = v.find("Name");
                if (name && name->is_object())
                {
                    const auto *ntr = name->find("Translate");
                    const auto *nto = name->find("TextOffset");
                    if (ntr && ntr->is_string() && nto && nto->is_int())
                    {
                        text_pack.modify(uint32_t(nto->as_int()), ntr->as_string(), target_cp);
                    }
                }
                const auto *text_obj = v.find("Text");
                if (!text_obj || !text_obj->is_object())
                    continue;
                const auto *tr = text_obj->find("Translate");
                const auto *to = text_obj->find("TextOffset");
                if (tr && tr->is_string() && to && to->is_int())
                {
                    text_pack.modify(uint32_t(to->as_int()), tr->as_string(), target_cp);
                }
            }

            std::vector<uint8_t> new_text;
            text_pack.rebuild_into(new_text, target_cp);
            if (!sp::write_file(out_text_path, new_text.data(), new_text.size()))
                return false;

            std::vector<uint8_t> new_script = script_bytes;
            for (const auto &v : arr)
            {
                const auto *so_v = v.find("ScriptOffset");
                const auto *text_obj = v.find("Text");
                if (!so_v || !so_v->is_int() || !text_obj || !text_obj->is_object())
                    continue;

                uint32_t script_off = uint32_t(so_v->as_int());
                auto it = op_offset_to_id.find(script_off);
                if (it == op_offset_to_id.end())
                    continue;
                const ScriptOp &op = ops[it->second];

                const auto *to_v = text_obj->find("TextOffset");
                if (!to_v || !to_v->is_int())
                    continue;
                int tid = text_pack.find_id_by_offset(uint32_t(to_v->as_int()));
                if (tid < 0)
                    continue;
                uint32_t new_text_off = text_pack.entries()[size_t(tid)].new_offset;

                if (op.kind == OpKind::TextShow)
                {
                    if (op.offset + 8 <= new_script.size())
                    {
                        sp::write_u32_le(new_script.data() + op.offset + 4, new_text_off);
                    }
                    const auto *name_v = v.find("Name");
                    if (name_v && name_v->is_object())
                    {
                        const auto *nto = name_v->find("TextOffset");
                        if (nto && nto->is_int())
                        {
                            int nid = text_pack.find_id_by_offset(uint32_t(nto->as_int()));
                            if (nid >= 0 && op.offset + 16 <= new_script.size())
                            {
                                uint32_t new_name_off = text_pack.entries()[size_t(nid)].new_offset;
                                sp::write_u32_le(new_script.data() + op.offset + 12, new_name_off);
                            }
                        }
                    }
                }
                else
                {
                    if (op.offset + 8 <= new_script.size())
                    {
                        sp::write_u32_le(new_script.data() + op.offset + 4, new_text_off);
                    }
                }
            }

            return sp::write_file(out_script_path, new_script.data(), new_script.size());
        }
    };

} // namespace sp::script

extern "C" SP_API int sp_script_to_json(const wchar_t *script_path,
                                        const wchar_t *text_path,
                                        const wchar_t *out_json_path)
{
    sp::clear_last_error();
    if (!script_path || !text_path || !out_json_path)
    {
        sp::set_last_error("sp_script_to_json: null argument.");
        return 0;
    }
    sp::script::Disassembler ds;
    if (!ds.load(script_path, text_path))
        return 0;
    return ds.export_json(out_json_path) ? 1 : 0;
}

extern "C" SP_API int sp_script_rebuild(const wchar_t *script_path,
                                        const wchar_t *text_path,
                                        const wchar_t *json_path,
                                        const wchar_t *out_script_path,
                                        const wchar_t *out_text_path,
                                        const char *target_encoding)
{
    sp::clear_last_error();
    if (!script_path || !text_path || !json_path || !out_script_path || !out_text_path)
    {
        sp::set_last_error("sp_script_rebuild: null argument.");
        return 0;
    }
    unsigned cp = sp::CP_GBK;
    if (target_encoding && *target_encoding)
    {
        std::string enc = target_encoding;
        for (auto &c : enc)
            c = char(std::tolower(uint8_t(c)));
        if (enc == "gbk")
            cp = sp::CP_GBK;
        else if (enc == "shift_jis" || enc == "sjis" || enc == "cp932")
            cp = sp::CP_SHIFT_JIS;
        else if (enc == "utf-8" || enc == "utf8")
            cp = sp::CP_UTF8_;
        else
        {
            sp::set_last_error_fmt("Unsupported target_encoding: %s", target_encoding);
            return 0;
        }
    }
    sp::script::Disassembler ds;
    if (!ds.load(script_path, text_path))
        return 0;
    return ds.rebuild(json_path, out_script_path, out_text_path, cp) ? 1 : 0;
}
