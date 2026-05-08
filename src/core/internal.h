#ifndef SOFTPAL_CORE_INTERNAL_H
#define SOFTPAL_CORE_INTERNAL_H

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace sp
{

    void set_last_error(std::string utf8);
    void set_last_error_fmt(const char *fmt, ...);
    void clear_last_error();
    const std::string &get_last_error_string();

    std::wstring utf8_to_wide(const std::string &utf8);
    std::string wide_to_utf8(const std::wstring &w);

    // 解码失败的字节会被替换为 "?"，不会抛错；调用方拿到的总是合法 UTF-8
    std::string decode_to_utf8(const std::string &bytes, unsigned cp);
    std::string encode_from_utf8(const std::string &utf8, unsigned cp);

    constexpr unsigned CP_SHIFT_JIS = 932;
    constexpr unsigned CP_GBK = 936;
    constexpr unsigned CP_UTF8_ = 65001;

    bool read_file(const std::filesystem::path &p, std::vector<uint8_t> &out);
    bool write_file(const std::filesystem::path &p, const void *data, size_t len);

    // 引擎所有二进制布局都按小端序，Windows 目标平台上无需运行时检测
    inline uint32_t read_u32_le(const uint8_t *p)
    {
        return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
    }
    inline void write_u32_le(uint8_t *p, uint32_t v)
    {
        p[0] = uint8_t(v & 0xff);
        p[1] = uint8_t((v >> 8) & 0xff);
        p[2] = uint8_t((v >> 16) & 0xff);
        p[3] = uint8_t((v >> 24) & 0xff);
    }

} // namespace sp

#endif // SOFTPAL_CORE_INTERNAL_H
