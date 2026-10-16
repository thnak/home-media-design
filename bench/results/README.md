# Kết quả benchmark

Mỗi lần chạy lưu một file `<mã>_<máy>_<YYYYMMDD>.csv` (ví dụ `B1_desktop-rtx4070_20261017.csv`). File kết quả thô và ảnh so sánh không đưa vào git; chỉ kết luận được ghi lại trong thiết kế.

Kết quả tham chiếu B1 trên máy ảo Linux (libsodium 1.0.18, AVX2 và AVX-512 có, 256 MiB mỗi kích thước), chỉ để kiểm tra benchmark chạy đúng:

| unit_kib | seal GiB/s | open GiB/s | overhead % |
| --- | --- | --- | --- |
| 64 | 0.74 | 1.12 | 0.0610 |
| 256 | 1.23 | 1.15 | 0.0153 |
| 1024 | 1.16 | 1.04 | 0.0038 |
| 4096 | 1.25 | 1.13 | 0.0010 |
| 16384 | 1.06 | 1.15 | 0.0002 |

Trên máy ảo này, từ 256 KiB trở lên thông lượng gần như phẳng, nên kích thước đơn vị nên chọn theo seek và mạng hơn là theo tốc độ mã hóa. Cần chạy lại trên server và client thật.
