# C++/CLI WinForms UI executable. Requires MSVC + .NET Framework 4.x.

if(NOT MSVC)
    message(FATAL_ERROR "softpal_ui requires MSVC for C++/CLI.")
endif()

add_executable(softpal_ui WIN32
    main.cpp
    MainForm.h
    app.rc
)

# C++/CLI mixed-mode flags. EHa is needed alongside /clr; OldSyntax matches the
# `gcnew` usage in MainForm.h.
set_target_properties(softpal_ui PROPERTIES
    OUTPUT_NAME "SoftPal-Sakura"
    COMMON_LANGUAGE_RUNTIME ""        # pure /clr (not /clr:pure)
    VS_DOTNET_TARGET_FRAMEWORK_VERSION "v4.8"
    VS_DOTNET_REFERENCES "System;System.Core;System.Drawing;System.Windows.Forms;System.Data"
)

# /clr is incompatible with /EHsc, /RTC1, /GR-, etc. Use /EHa for managed.
target_compile_options(softpal_ui PRIVATE
    /EHa
    /utf-8
    /Zc:__cplusplus
)

# Use /MD (dynamic CRT) — required by /clr.
set_property(TARGET softpal_ui PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")

# Link against the core DLL via its import library.
target_link_libraries(softpal_ui PRIVATE softpal_core)

# Place the core DLL next to the exe automatically.
add_custom_command(TARGET softpal_ui POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:softpal_core>"
            "$<TARGET_FILE_DIR:softpal_ui>/$<TARGET_FILE_NAME:softpal_core>"
    VERBATIM)
