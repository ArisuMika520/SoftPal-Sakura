#ifndef SOFTPAL_CORE_EXPORTS_H
#define SOFTPAL_CORE_EXPORTS_H

#include <stdint.h>
#include <wchar.h>

#ifdef _WIN32
#ifdef SOFTPAL_CORE_BUILD
#define SP_API __declspec(dllexport)
#else
#define SP_API __declspec(dllimport)
#endif
#else
#define SP_API
#endif

#ifdef __cplusplus
extern "C"
{
#endif

    // 线程局部存储；多线程调用方各自拥有独立的最近错误串
    SP_API const char *sp_last_error(void);

    typedef struct sp_pac_archive sp_pac_archive;

    typedef struct sp_pac_entry
    {
        char name[32];
        uint32_t size;
        uint32_t offset;
    } sp_pac_entry;

    SP_API sp_pac_archive *sp_pac_open(const wchar_t *path);
    SP_API void sp_pac_close(sp_pac_archive *arc);
    SP_API uint32_t sp_pac_count(sp_pac_archive *arc);
    SP_API int sp_pac_get_entry(sp_pac_archive *arc, uint32_t index, sp_pac_entry *out);

    SP_API int sp_pac_extract_index(sp_pac_archive *arc, uint32_t index, const wchar_t *out_dir);
    SP_API int sp_pac_extract_by_name(sp_pac_archive *arc, const char *name_ascii, const wchar_t *out_dir);

    typedef void (*sp_progress_cb)(uint32_t current, uint32_t total, const char *current_name_ascii, void *user);
    SP_API int sp_pac_extract_all(sp_pac_archive *arc, const wchar_t *out_dir, sp_progress_cb cb, void *user);

    // 与 pal_file_decrypt.py 一致的 ROL+XOR 解密；只用于较老版本的脚本
    SP_API int sp_pal_decrypt_file(const wchar_t *in_path, const wchar_t *out_path);

    // PGD 在本引擎里有两种：
    //   GE   : 底图，外层 40 字节头 (magic "GE \0")，承载完整 RGB(A) 像素
    //   PGD3 : 相对于某个 GE 底图的差分/叠图；头部含 cp932 NUL 结尾的底图文件名
    // PGD2 仅作为枚举占位，实际样本中尚未出现
    typedef enum sp_pgd_kind
    {
        SP_PGD_UNKNOWN = 0,
        SP_PGD_GE = 1,
        SP_PGD_PGD3 = 2,
        SP_PGD_PGD2 = 3,
    } sp_pgd_kind;

    typedef struct sp_pgd_info
    {
        sp_pgd_kind kind;
        uint32_t width;
        uint32_t height;
        uint32_t canvas_width; // PGD3 为父画布尺寸，GE 等于 width/height
        uint32_t canvas_height;
        uint32_t offset_x; // PGD3 在父画布中的左上角
        uint32_t offset_y;
        uint32_t channels;          // 仅 GE，3 表示 RGB；0 表示未知
        uint32_t uncompressed_size; // 仅 GE
        uint32_t compressed_size;
        uint32_t payload_offset;
        char base_name[64]; // 仅 PGD3：NUL 结尾的底图文件名
    } sp_pgd_info;

    SP_API int sp_pgd_inspect(const wchar_t *path, sp_pgd_info *info);

    // 解码 PGD 至 PNG。PGD3 需要同目录下存在底图，解码时会先加载底图再 XOR 合成
    SP_API int sp_pgd_to_png(const wchar_t *pgd_path, const wchar_t *png_path);

    // 递归遍历目录，把 *.PGD 转为同名 *.png
    //   delete_pgd : 转换成功后删除源 PGD
    //   cb         : 每个文件转换前回调
    // 单个文件失败会计入 *failed 但不中断整体流程
    SP_API int sp_dir_convert_pgd(const wchar_t *root,
                                  int delete_pgd,
                                  uint32_t *total,
                                  uint32_t *converted,
                                  uint32_t *failed,
                                  sp_progress_cb cb,
                                  void *user);

    SP_API int sp_script_to_json(const wchar_t *script_path,
                                 const wchar_t *text_path,
                                 const wchar_t *out_json_path);

    // target_encoding 传 NULL 或空串等价于 "gbk"
    SP_API int sp_script_rebuild(const wchar_t *script_path,
                                 const wchar_t *text_path,
                                 const wchar_t *json_path,
                                 const wchar_t *out_script_path,
                                 const wchar_t *out_text_path,
                                 const char *target_encoding);

#ifdef __cplusplus
}
#endif

#endif // SOFTPAL_CORE_EXPORTS_H
