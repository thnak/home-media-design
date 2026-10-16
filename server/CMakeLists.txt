# media_api: tiep xuc mang (REST, WebSocket, relay, SFU). KHONG giu khoa.
# Khong duoc link crypto_core; BoundaryCheck o CMakeLists goc se dung build neu vi pham.
add_executable(media_api api/main.cpp)
target_link_libraries(media_api PRIVATE hm::shared_protocol hm_compiler_options hm_avx2)

# media_worker: import, transcode GPU, metadata, key store. GIU KHOA trong RAM,
# khong mo cong ra Internet; noi chuyen voi media_api qua IPC cuc bo (muc 11).
add_executable(media_worker worker/main.cpp)
target_link_libraries(media_worker PRIVATE hm::crypto_core hm_compiler_options hm_avx2)
