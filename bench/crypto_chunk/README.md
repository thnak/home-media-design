# B1 — Thông lượng mã hóa theo kích thước đơn vị

Đo tốc độ `seal_unit_into` / `open_unit_into` (XChaCha20-Poly1305, AAD 39 byte) với các kích thước đơn vị từ 64 KiB đến 16 MiB.

```powershell
cmake --preset windows-release
cmake --build --preset windows-release --target bench_crypto_chunk
.\build\windows-release\bench\bench_crypto_chunk.exe 4096 > bench\results\B1_<may>_<ngay>.csv
```

Tham số là tổng dung lượng (MiB) cho mỗi kích thước; 4096 cho kết quả ổn định hơn. Dòng đầu trên stderr cho biết libsodium có dùng AVX2/AVX-512 không.

Cần chạy trên cả server và client yếu nhất (laptop, sau này Android). Kết luận dùng cho thiết kế mục 3.2:

- Kích thước đơn vị lưu trữ: nhỏ nhất mà thông lượng còn phẳng (seek chính xác hơn, retry rẻ hơn).
- Số đơn vị gộp mỗi request: quyết định bởi mạng, đo riêng khi có server API.
