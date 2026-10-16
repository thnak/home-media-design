# shared_protocol: dinh dang wire va dinh danh, khong chua khoa hay ma giai ma.
add_library(shared_protocol
    src/unit_id.cpp)
add_library(hm::shared_protocol ALIAS shared_protocol)
target_include_directories(shared_protocol PUBLIC include)
target_link_libraries(shared_protocol PRIVATE hm_compiler_options)
