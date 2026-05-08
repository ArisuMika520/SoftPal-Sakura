// PAL 解密：从偏移 16 起每 4 字节一组，shift 从 4 开始递增
//   byte[i]   <- ROL(byte[i], shift % 8)
//   dword[i]  <- dword[i] XOR 0xF7D5859D
// 循环边界取 len-4（与 pal_file_decrypt.py 的 range(16, len-4, 4) 一致），
// 不包括尾部四字节，故文件长度必须 >= 20 才执行解密
#include "core_exports.h"
#include "internal.h"

#include <filesystem>

namespace fs = std::filesystem;

namespace
{

    inline uint8_t rol8(uint8_t b, int shift)
    {
        shift &= 7;
        if (shift == 0)
            return b;
        return uint8_t((b << shift) | (b >> (8 - shift)));
    }

} // namespace

extern "C" SP_API int sp_pal_decrypt_file(const wchar_t *in_path, const wchar_t *out_path)
{
    sp::clear_last_error();
    if (!in_path || !out_path)
    {
        sp::set_last_error("sp_pal_decrypt_file: null path.");
        return 0;
    }
    std::vector<uint8_t> data;
    if (!sp::read_file(fs::path(in_path), data))
        return 0;

    constexpr uint32_t XOR_KEY = 0x084DF873u ^ 0xFF987DEEu;
    int shift = 4;
    if (data.size() >= 20)
    {
        const size_t end = data.size() - 4;
        for (size_t i = 16; i < end; i += 4)
        {
            data[i] = rol8(data[i], shift);
            uint32_t dw = sp::read_u32_le(data.data() + i);
            dw ^= XOR_KEY;
            sp::write_u32_le(data.data() + i, dw);
            ++shift;
        }
    }

    return sp::write_file(fs::path(out_path), data.data(), data.size()) ? 1 : 0;
}
