add_library(softpal_core SHARED
    core_exports.h
    internal.h
    util.cpp
    pac.cpp
    pal_decrypt.cpp
    pgd.cpp
    pgd_decode.cpp
    stb_image_write.h
    json_mini.h
    json_mini.cpp
    script.cpp
)

target_include_directories(softpal_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(softpal_core PRIVATE SOFTPAL_CORE_BUILD)

if(MSVC)
    target_compile_options(softpal_core PRIVATE /W3 /utf-8 /EHsc /Zc:__cplusplus)
endif()

set_target_properties(softpal_core PROPERTIES
    OUTPUT_NAME "SoftPal-Sakura"
    PREFIX ""
)
