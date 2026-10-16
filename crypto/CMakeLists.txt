# crypto_core: AEAD theo don vi, khoa con theo variant. GIU KHOA.
# Chi duoc link vao media_worker va media_client, khong bao gio vao media_api.
add_library(crypto_core
    src/unit_aead.cpp)
add_library(hm::crypto_core ALIAS crypto_core)
target_include_directories(crypto_core PUBLIC include)
target_link_libraries(crypto_core
    PUBLIC hm::shared_protocol
    PRIVATE hm_compiler_options unofficial-sodium::sodium)
