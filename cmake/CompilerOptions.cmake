# Tuy chon trinh bien dich dung chung cho moi target cua du an.
add_library(hm_compiler_options INTERFACE)

if(MSVC)
    target_compile_options(hm_compiler_options INTERFACE
        /W4 /permissive- /utf-8 /Zc:__cplusplus /EHsc)
    target_compile_definitions(hm_compiler_options INTERFACE
        NOMINMAX WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0A00)
    if(HM_WARNINGS_AS_ERRORS)
        target_compile_options(hm_compiler_options INTERFACE /WX)
    endif()
else()
    target_compile_options(hm_compiler_options INTERFACE
        -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    if(HM_WARNINGS_AS_ERRORS)
        target_compile_options(hm_compiler_options INTERFACE -Werror)
    endif()
endif()

# Muc 1: server yeu cau AVX2. Chi bat cho target server, client van chay tren may cu.
add_library(hm_avx2 INTERFACE)
if(MSVC)
    target_compile_options(hm_avx2 INTERFACE /arch:AVX2)
else()
    target_compile_options(hm_avx2 INTERFACE -mavx2 -mfma)
endif()
