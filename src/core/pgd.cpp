// PGD 头部布局速查（pixel codec 在 pgd_decode.cpp，下表只列头字段位置）
//
// GE PGD（底图，40 字节外层头）：
//   0x00  "GE \0" magic
//   0x0C  width / 0x10 height / 0x14 width / 0x18 height（后两次重复）
//   0x1C  channels (观察到 3) / 0x20 解压后大小 / 0x24 压缩流大小
//   0x28  之后为自定义编码的像素流
//
// PGD3（对某 GE 底图的差分叠图）：
//   0x00  "PGD3" magic
//   0x04  field_a (u16，含义未确认)
//   0x06  width / 0x08 height / 0x0A offset_x / 0x0C offset_y (均 u16)
//   0x0E  底图文件名（cp932 ASCII，NUL 结尾），其后为差分流
#include "core_exports.h"
#include "internal.h"

#include <cstring>
#include <filesystem>

namespace fs = std::filesystem;

extern "C" SP_API int sp_pgd_inspect(const wchar_t *path, sp_pgd_info *info)
{
    sp::clear_last_error();
    if (!path || !info)
    {
        sp::set_last_error("sp_pgd_inspect: null argument.");
        return 0;
    }
    std::memset(info, 0, sizeof(*info));

    std::vector<uint8_t> data;
    if (!sp::read_file(fs::path(path), data))
        return 0;
    if (data.size() < 4)
    {
        sp::set_last_error("PGD too small.");
        return 0;
    }

    if (data.size() >= 0x28 && std::memcmp(data.data(), "GE \0", 4) == 0)
    {
        info->kind = SP_PGD_GE;
        info->width = sp::read_u32_le(data.data() + 0x0C);
        info->height = sp::read_u32_le(data.data() + 0x10);
        info->canvas_width = sp::read_u32_le(data.data() + 0x14);
        info->canvas_height = sp::read_u32_le(data.data() + 0x18);
        info->channels = sp::read_u32_le(data.data() + 0x1C);
        info->uncompressed_size = sp::read_u32_le(data.data() + 0x20);
        info->compressed_size = sp::read_u32_le(data.data() + 0x24);
        info->payload_offset = 0x28;
        return 1;
    }

    if (data.size() >= 0x10 && std::memcmp(data.data(), "PGD3", 4) == 0)
    {
        info->kind = SP_PGD_PGD3;
        info->width = uint32_t(data[0x06]) | (uint32_t(data[0x07]) << 8);
        info->height = uint32_t(data[0x08]) | (uint32_t(data[0x09]) << 8);
        info->offset_x = uint32_t(data[0x0A]) | (uint32_t(data[0x0B]) << 8);
        info->offset_y = uint32_t(data[0x0C]) | (uint32_t(data[0x0D]) << 8);
        size_t name_start = 0x0E;
        size_t name_end = name_start;
        while (name_end < data.size() && name_end < name_start + sizeof(info->base_name) - 1 && data[name_end] != 0)
        {
            ++name_end;
        }
        size_t name_len = name_end - name_start;
        std::memcpy(info->base_name, data.data() + name_start, name_len);
        info->base_name[name_len] = 0;
        info->payload_offset = uint32_t(name_end + 1);
        info->compressed_size = (data.size() > info->payload_offset)
                                    ? uint32_t(data.size() - info->payload_offset)
                                    : 0;
        return 1;
    }

    // PGD2 仅在 Pal.dll 字符串中出现，本测试样本未见过实例
    if (data.size() >= 4 && std::memcmp(data.data(), "PGD2", 4) == 0)
    {
        info->kind = SP_PGD_PGD2;
        info->compressed_size = uint32_t(data.size());
        return 1;
    }

    info->kind = SP_PGD_UNKNOWN;
    sp::set_last_error_fmt("Unknown PGD magic: %02x %02x %02x %02x",
                           data[0], data[1], data[2], data[3]);
    return 0;
}
