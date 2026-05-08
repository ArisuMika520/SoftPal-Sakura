// 解码逻辑移植自 GARbro 的 ImagePGD.cs（Softpal / AmuseCraft 分支），
// 修改自定义 LZ77 / 行内差分算法的实现细节请对照上游
#include "core_exports.h"
#include "internal.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WRITE_NO_STDIO
#include "stb_image_write.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace sp::pgd
{

    struct DecodeError : std::runtime_error
    {
        using std::runtime_error::runtime_error;
    };

    class Cursor
    {
    public:
        Cursor(const uint8_t *data, size_t size) : data_(data), size_(size) {}

        uint8_t read_u8()
        {
            if (pos_ >= size_)
                throw DecodeError("PGD: unexpected EOF");
            return data_[pos_++];
        }
        uint16_t read_u16()
        {
            if (pos_ + 2 > size_)
                throw DecodeError("PGD: unexpected EOF");
            uint16_t v = uint16_t(data_[pos_]) | (uint16_t(data_[pos_ + 1]) << 8);
            pos_ += 2;
            return v;
        }
        uint32_t read_u32()
        {
            if (pos_ + 4 > size_)
                throw DecodeError("PGD: unexpected EOF");
            uint32_t v = sp::read_u32_le(data_ + pos_);
            pos_ += 4;
            return v;
        }
        void read_into(std::vector<uint8_t> &dst, size_t at, size_t n)
        {
            if (pos_ + n > size_)
                throw DecodeError("PGD: unexpected EOF");
            std::memcpy(dst.data() + at, data_ + pos_, n);
            pos_ += n;
        }
        size_t pos() const { return pos_; }
        void seek(size_t p) { pos_ = p; }

    private:
        const uint8_t *data_;
        size_t size_;
        size_t pos_ = 0;
    };

    // out 由调用者按外层头声明的 unpacked_size 预分配
    void unpack_ge_pre(Cursor &in, std::vector<uint8_t> &out)
    {
        size_t dst = 0;
        int ctl = 2;
        while (dst < out.size())
        {
            ctl >>= 1;
            if (ctl == 1)
            {
                ctl = int(in.read_u8()) | 0x100;
            }
            if ((ctl & 1) != 0)
            {
                uint16_t offset = in.read_u16();
                int count = offset & 7;
                if ((offset & 8) == 0)
                {
                    count = (count << 8) | in.read_u8();
                }
                count += 4;
                int back = offset >> 4;
                if (back == 0 || size_t(back) > dst)
                {
                    throw DecodeError("PGD: bad LZ back reference");
                }
                if (dst + size_t(count) > out.size())
                {
                    count = int(out.size() - dst);
                }
                // 逐字节复制以支持自重叠引用 (back < count 的情况)
                for (int i = 0; i < count; ++i)
                {
                    out[dst] = out[dst - size_t(back)];
                    ++dst;
                }
            }
            else
            {
                int count = in.read_u8();
                if (dst + size_t(count) > out.size())
                {
                    count = int(out.size() - dst);
                }
                in.read_into(out, dst, size_t(count));
                dst += size_t(count);
            }
        }
    }

    // 行差分还原。pixel_size 为像素字节数 (BGR24=3 / BGRA32=4)。
    // src 之后紧跟 height 字节的逐行模式标志，再跟实际差分流
    std::vector<uint8_t> post_process_pal(const std::vector<uint8_t> &input,
                                          size_t src,
                                          int width,
                                          int height,
                                          int pixel_size)
    {
        const size_t stride = size_t(width) * size_t(pixel_size);
        std::vector<uint8_t> out(stride * size_t(height), 0);

        size_t ctl_p = src;
        src += size_t(height);
        size_t dst = 0;

        auto need_in = [&](size_t n)
        {
            if (src + n > input.size())
                throw DecodeError("PGD: truncated PAL stream");
        };

        for (int row = 0; row < height; ++row)
        {
            if (ctl_p >= input.size())
                throw DecodeError("PGD: truncated PAL ctl");
            uint8_t c = input[ctl_p++];

            if (c & 1)
            {
                // 行内差分：种子像素 + 对前一像素求差
                need_in(size_t(pixel_size));
                size_t prev = dst;
                std::memcpy(out.data() + dst, input.data() + src, size_t(pixel_size));
                src += size_t(pixel_size);
                dst += size_t(pixel_size);
                size_t count = stride - size_t(pixel_size);
                need_in(count);
                for (size_t i = 0; i < count; ++i)
                {
                    out[dst++] = uint8_t(out[prev++] - input[src++]);
                }
            }
            else if (c & 2)
            {
                // 行间差分：与上一行同列对差
                size_t prev = dst - stride;
                size_t count = stride;
                need_in(count);
                for (size_t i = 0; i < count; ++i)
                {
                    out[dst++] = uint8_t(out[prev++] - input[src++]);
                }
            }
            else
            {
                // 二维平均预测：种子 + 与 (左 + 上)/2 的差
                need_in(size_t(pixel_size));
                std::memcpy(out.data() + dst, input.data() + src, size_t(pixel_size));
                dst += size_t(pixel_size);
                src += size_t(pixel_size);
                size_t prev = dst - stride;
                size_t count = stride - size_t(pixel_size);
                need_in(count);
                for (size_t i = 0; i < count; ++i)
                {
                    int avg = (int(out[prev++]) + int(out[dst - size_t(pixel_size)])) / 2;
                    out[dst] = uint8_t(avg - int(input[src++]));
                    ++dst;
                }
            }
        }
        return out;
    }

    std::vector<uint8_t> post_process_3(const std::vector<uint8_t> &raw,
                                        int &out_w,
                                        int &out_h,
                                        int &out_pixel_size)
    {
        if (raw.size() < 8)
            throw DecodeError("PGD: PostProcess3 needs >= 8 bytes");
        uint16_t bpp = uint16_t(raw[2]) | (uint16_t(raw[3]) << 8);
        if (bpp != 24 && bpp != 32)
        {
            throw DecodeError("PGD: unsupported BPP in PostProcess3");
        }
        out_w = uint16_t(raw[4]) | (uint16_t(raw[5]) << 8);
        out_h = uint16_t(raw[6]) | (uint16_t(raw[7]) << 8);
        out_pixel_size = bpp / 8;
        return post_process_pal(raw, 8, out_w, out_h, out_pixel_size);
    }

    struct DecodedImage
    {
        int width;
        int height;
        int pixel_size;              // BGR24=3 / BGRA32=4
        std::vector<uint8_t> pixels; // top-down BGR(A)
    };

    DecodedImage decode_ge(const std::vector<uint8_t> &file_bytes)
    {
        if (file_bytes.size() < 0x28 || std::memcmp(file_bytes.data(), "GE \0", 4) != 0)
        {
            throw DecodeError("PGD: not a GE PGD");
        }
        // 头部 0x1C 处的 method 字段；method 1/2 在测试样本里未出现，遇到时直接报错
        uint16_t method = uint16_t(file_bytes[0x1C]) | (uint16_t(file_bytes[0x1D]) << 8);
        if (method != 3)
        {
            throw DecodeError("PGD: GE method " + std::to_string(method) + " not supported (only 3)");
        }
        uint32_t unpacked_size = sp::read_u32_le(file_bytes.data() + 0x20);
        Cursor in(file_bytes.data(), file_bytes.size());
        in.seek(0x28);
        std::vector<uint8_t> raw(unpacked_size, 0);
        unpack_ge_pre(in, raw);

        DecodedImage img;
        img.pixels = post_process_3(raw, img.width, img.height, img.pixel_size);
        return img;
    }

    DecodedImage decode_overlay(const std::vector<uint8_t> &file_bytes,
                                const fs::path &pgd_path)
    {
        if (file_bytes.size() < 0x30 || std::memcmp(file_bytes.data(), "PGD3", 4) != 0)
        {
            throw DecodeError("PGD: not a PGD3 overlay");
        }
        auto u16_at = [&](size_t off)
        {
            return uint16_t(file_bytes[off]) | (uint16_t(file_bytes[off + 1]) << 8);
        };
        uint16_t off_x = u16_at(0x04);
        uint16_t off_y = u16_at(0x06);
        uint16_t width = u16_at(0x08);
        uint16_t height = u16_at(0x0A);
        uint16_t bpp = u16_at(0x0C);
        if (bpp != 24 && bpp != 32)
        {
            throw DecodeError("PGD: PGD3 unsupported BPP " + std::to_string(bpp));
        }
        int overlay_pixel_size = bpp / 8;

        // 底图文件名存放在 0x0E，NUL 结尾，GARbro 上限取 0x22 字节
        std::string base_name;
        for (size_t i = 0x0E; i < 0x30 && i < file_bytes.size(); ++i)
        {
            if (file_bytes[i] == 0)
                break;
            base_name.push_back(char(file_bytes[i]));
        }
        if (base_name.empty())
            throw DecodeError("PGD: PGD3 missing base name");

        fs::path base_path = pgd_path.parent_path() / fs::path(sp::utf8_to_wide(base_name));
        std::vector<uint8_t> base_bytes;
        if (!sp::read_file(base_path, base_bytes))
        {
            // 部分样本中底图文件名后缀大小写不一致，回退到 .PGD 再试一次
            fs::path alt = pgd_path.parent_path() /
                           fs::path(sp::utf8_to_wide(base_name)).replace_extension(L".PGD");
            if (!sp::read_file(alt, base_bytes))
            {
                throw DecodeError("PGD: cannot open base " + base_name);
            }
        }
        DecodedImage base = decode_ge(base_bytes);
        if (off_x + width > base.width || off_y + height > base.height)
        {
            throw DecodeError("PGD: PGD3 patch exceeds base dimensions");
        }

        // PGD3 的 PgdIncMetaData 从 0x30 开始，前 8 字节为 unpacked / packed 长度，
        // 真正的 LZ 流从 0x38 起
        if (file_bytes.size() < 0x38)
            throw DecodeError("PGD: PGD3 truncated header");
        uint32_t unpacked_size = sp::read_u32_le(file_bytes.data() + 0x30);
        Cursor in(file_bytes.data(), file_bytes.size());
        in.seek(0x38);
        std::vector<uint8_t> raw(unpacked_size, 0);
        unpack_ge_pre(in, raw);
        std::vector<uint8_t> overlay = post_process_pal(raw, 0, width, height, overlay_pixel_size);

        bool apply_alpha = (overlay_pixel_size == 4) && (base.pixel_size == 4);
        int base_bpp_ = base.pixel_size;
        int gap = (base.width - width) * base_bpp_;
        size_t dst = (size_t(off_y) * size_t(base.width) + size_t(off_x)) * size_t(base_bpp_);
        size_t src = 0;
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                base.pixels[dst] ^= overlay[src];
                base.pixels[dst + 1] ^= overlay[src + 1];
                base.pixels[dst + 2] ^= overlay[src + 2];
                if (apply_alpha)
                    base.pixels[dst + 3] ^= overlay[src + 3];
                dst += size_t(base_bpp_);
                src += size_t(overlay_pixel_size);
            }
            dst += size_t(gap);
        }
        return base;
    }

    DecodedImage decode_file(const fs::path &pgd_path)
    {
        std::vector<uint8_t> file_bytes;
        if (!sp::read_file(pgd_path, file_bytes))
        {
            throw DecodeError("PGD: cannot read input file");
        }
        if (file_bytes.size() < 4)
            throw DecodeError("PGD: empty file");
        if (std::memcmp(file_bytes.data(), "GE \0", 4) == 0)
        {
            return decode_ge(file_bytes);
        }
        if (std::memcmp(file_bytes.data(), "PGD3", 4) == 0)
        {
            return decode_overlay(file_bytes, pgd_path);
        }
        throw DecodeError("PGD: unknown magic");
    }

    // stb_image_write 需要 RGB(A) 顺序，原始解码输出是 BGR(A)
    void bgr_to_rgb(std::vector<uint8_t> &pixels, int pixel_size)
    {
        for (size_t i = 0; i + size_t(pixel_size) <= pixels.size(); i += size_t(pixel_size))
        {
            std::swap(pixels[i], pixels[i + 2]);
        }
    }

} // namespace sp::pgd

namespace
{
    struct PngSink
    {
        std::vector<uint8_t> *out;
    };
    void png_writer(void *ctx, void *data, int len)
    {
        auto *sink = static_cast<PngSink *>(ctx);
        auto *p = static_cast<uint8_t *>(data);
        sink->out->insert(sink->out->end(), p, p + len);
    }
} // namespace

// 两阶段转换：先处理 GE 底图，再处理 PGD3 叠图，并把删除推迟到最后。
// 因为 PGD3 解码时需要按文件名读取同目录的 GE 底图，提前删除会破坏依赖。
// 单个文件失败不会中断流程，失败者会保留在原地以便排查
extern "C" SP_API int sp_dir_convert_pgd(const wchar_t *root,
                                         int delete_pgd,
                                         uint32_t *total,
                                         uint32_t *converted,
                                         uint32_t *failed,
                                         sp_progress_cb cb,
                                         void *user)
{
    sp::clear_last_error();
    if (total)
        *total = 0;
    if (converted)
        *converted = 0;
    if (failed)
        *failed = 0;
    if (!root)
    {
        sp::set_last_error("sp_dir_convert_pgd: null root.");
        return 0;
    }

    fs::path root_path(root);
    if (!fs::exists(root_path) || !fs::is_directory(root_path))
    {
        sp::set_last_error_fmt("Not a directory: %ls", root);
        return 0;
    }

    std::vector<fs::path> ge_files;
    std::vector<fs::path> pgd3_files;
    for (auto &entry : fs::recursive_directory_iterator(root_path))
    {
        if (!entry.is_regular_file())
            continue;
        auto p = entry.path();
        std::wstring ext = p.extension().wstring();
        for (auto &c : ext)
            c = wchar_t(towlower(c));
        if (ext != L".pgd")
            continue;

        std::ifstream f(p, std::ios::binary);
        char magic[4]{};
        f.read(magic, 4);
        if (!f)
            continue;
        if (std::memcmp(magic, "GE \0", 4) == 0)
            ge_files.push_back(p);
        else if (std::memcmp(magic, "PGD3", 4) == 0)
            pgd3_files.push_back(p);
    }

    uint32_t local_total = uint32_t(ge_files.size() + pgd3_files.size());
    uint32_t local_ok = 0, local_fail = 0;
    if (total)
        *total = local_total;

    auto convert_one = [&](const fs::path &pgd)
    {
        try
        {
            auto img = sp::pgd::decode_file(pgd);
            sp::pgd::bgr_to_rgb(img.pixels, img.pixel_size);
            std::vector<uint8_t> png;
            png.reserve(img.pixels.size() / 4);
            PngSink sink{&png};
            int stride = img.width * img.pixel_size;
            int ok = stbi_write_png_to_func(&png_writer, &sink,
                                            img.width, img.height,
                                            img.pixel_size,
                                            img.pixels.data(), stride);
            if (!ok || png.empty())
                return false;
            fs::path out = pgd;
            out.replace_extension(L".png");
            return sp::write_file(out, png.data(), png.size());
        }
        catch (const std::exception &)
        {
            return false;
        }
    };

    std::vector<fs::path> succeeded;
    succeeded.reserve(ge_files.size() + pgd3_files.size());

    uint32_t idx = 0;
    auto report = [&](const fs::path &p)
    {
        if (cb)
        {
            std::string utf8 = sp::wide_to_utf8(p.filename().wstring());
            cb(idx, local_total, utf8.c_str(), user);
        }
        ++idx;
    };

    for (auto &p : ge_files)
    {
        report(p);
        if (convert_one(p))
        {
            ++local_ok;
            succeeded.push_back(p);
        }
        else
        {
            ++local_fail;
        }
    }
    for (auto &p : pgd3_files)
    {
        report(p);
        if (convert_one(p))
        {
            ++local_ok;
            succeeded.push_back(p);
        }
        else
        {
            ++local_fail;
        }
    }
    if (cb)
        cb(local_total, local_total, "", user);
    if (delete_pgd)
    {
        for (auto &p : succeeded)
        {
            std::error_code ec;
            fs::remove(p, ec);
        }
    }

    if (converted)
        *converted = local_ok;
    if (failed)
        *failed = local_fail;
    return 1;
}

extern "C" SP_API int sp_pgd_to_png(const wchar_t *pgd_path, const wchar_t *png_path)
{
    sp::clear_last_error();
    if (!pgd_path || !png_path)
    {
        sp::set_last_error("sp_pgd_to_png: null argument.");
        return 0;
    }
    try
    {
        auto img = sp::pgd::decode_file(fs::path(pgd_path));
        sp::pgd::bgr_to_rgb(img.pixels, img.pixel_size);

        std::vector<uint8_t> png;
        png.reserve(img.pixels.size() / 4);
        PngSink sink{&png};
        int stride = img.width * img.pixel_size;
        int ok = stbi_write_png_to_func(&png_writer, &sink,
                                        img.width, img.height,
                                        img.pixel_size,
                                        img.pixels.data(), stride);
        if (!ok || png.empty())
        {
            sp::set_last_error("PGD: PNG encode failed");
            return 0;
        }
        return sp::write_file(fs::path(png_path), png.data(), png.size()) ? 1 : 0;
    }
    catch (const std::exception &e)
    {
        sp::set_last_error(e.what());
        return 0;
    }
}
