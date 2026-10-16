# Framework test (GoogleTest hay Catch2) chua chot, xem thiet ke muc 12.
# Tam thoi dung test tu chua, chay qua CTest.
add_executable(test_unit_aead unit/test_unit_aead.cpp)
target_link_libraries(test_unit_aead PRIVATE hm::crypto_core hm_compiler_options)
add_test(NAME unit_aead COMMAND test_unit_aead)
