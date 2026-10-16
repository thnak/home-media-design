# media_client: ung dung Windows (sau nay Android qua JNI). Giu khoa cua thiet bi.
add_executable(media_client app/main.cpp)
target_link_libraries(media_client PRIVATE hm::crypto_core hm_compiler_options)
