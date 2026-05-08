#include "core_exports.h"
#include "internal.h"

#include <windows.h>

#include <cstdarg>
#include <cstdio>
#include <fstream>

namespace sp
{

    namespace
    {
        thread_local std::string g_last_error;
    }

    void set_last_error(std::string utf8) { g_last_error = std::move(utf8); }
    void clear_last_error() { g_last_error.clear(); }
    const std::string &get_last_error_string() { return g_last_error; }

    void set_last_error_fmt(const char *fmt, ...)
    {
        char buf[1024];
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);
        g_last_error.assign(buf);
    }

    std::wstring utf8_to_wide(const std::string &utf8)
    {
        if (utf8.empty())
            return {};
        int n = ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(), int(utf8.size()), nullptr, 0);
        if (n <= 0)
            return {};
        std::wstring out(size_t(n), L'\0');
        ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(), int(utf8.size()), out.data(), n);
        return out;
    }

    std::string wide_to_utf8(const std::wstring &w)
    {
        if (w.empty())
            return {};
        int n = ::WideCharToMultiByte(CP_UTF8, 0, w.data(), int(w.size()), nullptr, 0, nullptr, nullptr);
        if (n <= 0)
            return {};
        std::string out(size_t(n), '\0');
        ::WideCharToMultiByte(CP_UTF8, 0, w.data(), int(w.size()), out.data(), n, nullptr, nullptr);
        return out;
    }

    std::string decode_to_utf8(const std::string &bytes, unsigned cp)
    {
        if (bytes.empty())
            return {};
        int n = ::MultiByteToWideChar(cp, 0, bytes.data(), int(bytes.size()), nullptr, 0);
        if (n <= 0)
        {
            // 默认模式失败时改用严格校验再试一次，避免直接返回空串
            n = ::MultiByteToWideChar(cp, MB_ERR_INVALID_CHARS, bytes.data(), int(bytes.size()), nullptr, 0);
            if (n <= 0)
                return {};
        }
        std::wstring w(size_t(n), L'\0');
        ::MultiByteToWideChar(cp, 0, bytes.data(), int(bytes.size()), w.data(), n);
        return wide_to_utf8(w);
    }

    std::string encode_from_utf8(const std::string &utf8, unsigned cp)
    {
        if (utf8.empty())
            return {};
        std::wstring w = utf8_to_wide(utf8);
        BOOL used_default = FALSE;
        char default_char = '?';
        int n = ::WideCharToMultiByte(cp, 0, w.data(), int(w.size()), nullptr, 0,
                                      &default_char, &used_default);
        if (n <= 0)
            return {};
        std::string out(size_t(n), '\0');
        ::WideCharToMultiByte(cp, 0, w.data(), int(w.size()), out.data(), n,
                              &default_char, &used_default);
        return out;
    }

    bool read_file(const std::filesystem::path &p, std::vector<uint8_t> &out)
    {
        std::ifstream f(p, std::ios::binary | std::ios::ate);
        if (!f)
        {
            set_last_error_fmt("Cannot open file for reading: %ls", p.c_str());
            return false;
        }
        auto size = f.tellg();
        if (size < 0)
        {
            set_last_error_fmt("Failed to query size: %ls", p.c_str());
            return false;
        }
        out.resize(size_t(size));
        f.seekg(0);
        if (size > 0)
            f.read(reinterpret_cast<char *>(out.data()), size);
        if (!f)
        {
            set_last_error_fmt("Failed to read file: %ls", p.c_str());
            return false;
        }
        return true;
    }

    bool write_file(const std::filesystem::path &p, const void *data, size_t len)
    {
        if (p.has_parent_path())
        {
            std::error_code ec;
            std::filesystem::create_directories(p.parent_path(), ec);
        }
        std::ofstream f(p, std::ios::binary | std::ios::trunc);
        if (!f)
        {
            set_last_error_fmt("Cannot open file for writing: %ls", p.c_str());
            return false;
        }
        if (len > 0)
            f.write(reinterpret_cast<const char *>(data), std::streamsize(len));
        if (!f)
        {
            set_last_error_fmt("Failed to write file: %ls", p.c_str());
            return false;
        }
        return true;
    }

} // namespace sp

extern "C" SP_API const char *sp_last_error(void)
{
    return sp::get_last_error_string().c_str();
}
