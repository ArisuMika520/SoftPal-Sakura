// PAC 归档结构（小端序）：
//   [2052, file_list_end) 为条目表，每条 40 字节：
//       [0..32)  ASCII 文件名（0 填充）
//       [32..36) size
//       [36..40) offset
//   首条目的 offset（位于文件偏移 2088）即为整个条目表的结束位置
#include "core_exports.h"
#include "internal.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

namespace fs = std::filesystem;

namespace sp
{

    struct PacArchive
    {
        fs::path path;
        std::ifstream stream;
        std::vector<sp_pac_entry> entries;
    };

    namespace
    {

        bool load_entries(PacArchive &arc)
        {
            arc.stream.seekg(2088);
            uint8_t buf4[4];
            arc.stream.read(reinterpret_cast<char *>(buf4), 4);
            if (!arc.stream)
            {
                set_last_error("Failed to read PAC header (file too small).");
                return false;
            }
            uint32_t file_list_end = read_u32_le(buf4);
            constexpr uint32_t file_list_start = 2052;
            if (file_list_end < file_list_start)
            {
                set_last_error_fmt("Corrupt PAC: file list end (%u) < start (%u).",
                                   file_list_end, file_list_start);
                return false;
            }
            if ((file_list_end - file_list_start) % 40 != 0)
            {
                set_last_error_fmt("Corrupt PAC: file list span %u not multiple of 40.",
                                   file_list_end - file_list_start);
                return false;
            }

            uint32_t count = (file_list_end - file_list_start) / 40;
            arc.entries.clear();
            arc.entries.reserve(count);

            arc.stream.seekg(file_list_start);
            for (uint32_t i = 0; i < count; ++i)
            {
                uint8_t row[40];
                arc.stream.read(reinterpret_cast<char *>(row), 40);
                if (!arc.stream)
                {
                    set_last_error_fmt("Truncated PAC entry table at index %u.", i);
                    return false;
                }
                sp_pac_entry e{};
                size_t namelen = 0;
                while (namelen < 32 && row[namelen] != 0)
                    ++namelen;
                std::memcpy(e.name, row, namelen);
                e.name[namelen] = 0;
                e.size = read_u32_le(row + 32);
                e.offset = read_u32_le(row + 36);
                arc.entries.push_back(e);
            }
            return true;
        }

        bool extract_entry(PacArchive &arc, const sp_pac_entry &e, const fs::path &out_dir)
        {
            std::error_code ec;
            fs::create_directories(out_dir, ec);
            fs::path out_path = out_dir / e.name;

            arc.stream.clear();
            arc.stream.seekg(e.offset);
            if (!arc.stream)
            {
                set_last_error_fmt("Cannot seek to offset %u for entry %s", e.offset, e.name);
                return false;
            }

            std::vector<uint8_t> buf(e.size);
            if (e.size > 0)
            {
                arc.stream.read(reinterpret_cast<char *>(buf.data()), e.size);
                if (!arc.stream)
                {
                    set_last_error_fmt("Truncated read for entry %s (%u bytes).", e.name, e.size);
                    return false;
                }
            }
            return write_file(out_path, buf.data(), buf.size());
        }

    } // namespace

} // namespace sp

using sp::PacArchive;

extern "C" SP_API sp_pac_archive *sp_pac_open(const wchar_t *path)
{
    sp::clear_last_error();
    if (!path || !*path)
    {
        sp::set_last_error("sp_pac_open: empty path.");
        return nullptr;
    }
    auto *arc = new PacArchive();
    arc->path = path;
    arc->stream.open(arc->path, std::ios::binary);
    if (!arc->stream)
    {
        sp::set_last_error_fmt("Cannot open PAC: %ls", path);
        delete arc;
        return nullptr;
    }
    if (!sp::load_entries(*arc))
    {
        delete arc;
        return nullptr;
    }
    return reinterpret_cast<sp_pac_archive *>(arc);
}

extern "C" SP_API void sp_pac_close(sp_pac_archive *arc_)
{
    delete reinterpret_cast<PacArchive *>(arc_);
}

extern "C" SP_API uint32_t sp_pac_count(sp_pac_archive *arc_)
{
    if (!arc_)
        return 0;
    auto *arc = reinterpret_cast<PacArchive *>(arc_);
    return uint32_t(arc->entries.size());
}

extern "C" SP_API int sp_pac_get_entry(sp_pac_archive *arc_, uint32_t index, sp_pac_entry *out)
{
    sp::clear_last_error();
    if (!arc_ || !out)
        return 0;
    auto *arc = reinterpret_cast<PacArchive *>(arc_);
    if (index >= arc->entries.size())
    {
        sp::set_last_error_fmt("Index %u out of range (count %zu).", index, arc->entries.size());
        return 0;
    }
    *out = arc->entries[index];
    return 1;
}

extern "C" SP_API int sp_pac_extract_index(sp_pac_archive *arc_, uint32_t index, const wchar_t *out_dir)
{
    sp::clear_last_error();
    if (!arc_ || !out_dir)
    {
        sp::set_last_error("sp_pac_extract_index: invalid argument.");
        return 0;
    }
    auto *arc = reinterpret_cast<PacArchive *>(arc_);
    if (index >= arc->entries.size())
    {
        sp::set_last_error_fmt("Index %u out of range.", index);
        return 0;
    }
    return sp::extract_entry(*arc, arc->entries[index], out_dir) ? 1 : 0;
}

extern "C" SP_API int sp_pac_extract_by_name(sp_pac_archive *arc_, const char *name_ascii, const wchar_t *out_dir)
{
    sp::clear_last_error();
    if (!arc_ || !name_ascii || !out_dir)
    {
        sp::set_last_error("sp_pac_extract_by_name: invalid argument.");
        return 0;
    }
    auto *arc = reinterpret_cast<PacArchive *>(arc_);
    for (const auto &e : arc->entries)
    {
        if (std::strncmp(e.name, name_ascii, sizeof(e.name)) == 0)
        {
            return sp::extract_entry(*arc, e, out_dir) ? 1 : 0;
        }
    }
    sp::set_last_error_fmt("Entry not found: %s", name_ascii);
    return 0;
}

extern "C" SP_API int sp_pac_extract_all(sp_pac_archive *arc_, const wchar_t *out_dir,
                                         sp_progress_cb cb, void *user)
{
    sp::clear_last_error();
    if (!arc_ || !out_dir)
    {
        sp::set_last_error("sp_pac_extract_all: invalid argument.");
        return 0;
    }
    auto *arc = reinterpret_cast<PacArchive *>(arc_);
    uint32_t total = uint32_t(arc->entries.size());
    for (uint32_t i = 0; i < total; ++i)
    {
        const auto &e = arc->entries[i];
        if (cb)
            cb(i, total, e.name, user);
        if (!sp::extract_entry(*arc, e, out_dir))
            return 0;
    }
    if (cb)
        cb(total, total, "", user);
    return 1;
}
