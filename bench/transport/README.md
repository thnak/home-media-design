# B5 — Truyền tin cậy trên UDP: SCTP trên Noise so với KCP

Prototype C++ nhỏ để kiểm chứng lớp truyền đề xuất ở thiết kế mục 9.8 trước giai đoạn 4. Chưa viết.

## Phạm vi

1. Hai tiến trình (client, server) trao gói UDP; mỗi gói là một khung Noise `IKpsk2` (khóa cố định trong prototype).
2. Bên trong khung: usrsctp qua `AF_CONN`, MTU SCTP khoảng 1200 byte. Phương án so sánh: KCP, mỗi luồng một instance, chế độ bình thường.
3. Luồng thử: một luồng tải đơn vị 1 MiB liên tục, một luồng control gửi tin nhỏ mỗi 100 ms, một luồng presence PR-SCTP.
4. Giả lập mạng: `tc netem` trên Linux, clumsy trên Windows.

## Kịch bản

| Kịch bản | Mạng | Đo |
| --- | --- | --- |
| LAN | Gigabit, không mất gói | Thông lượng một kết nối, CPU mỗi bên |
| Từ xa | 50 Mbps, RTT 60 ms, mất 1–2% | Thông lượng; độ trễ luồng control khi luồng tải đang chạy hết tốc độ |
| Công bằng | Như trên, chạy song song một luồng TCP và một luồng giả lập voice Opus 32 kbps | Băng thông chia cho từng luồng; độ trễ và jitter của voice |
| Đổi mạng | Đổi địa chỉ nguồn của client giữa chừng | Thời gian khôi phục, có mất dữ liệu không |

## Tiêu chí đạt

- LAN: một kết nối đạt ít nhất 500 Mbps mà CPU chưa bão hòa.
- Từ xa: dùng được ít nhất 80% băng thông giả lập; tin control trễ không quá RTT + 50 ms khi luồng tải đầy.
- Công bằng: không làm luồng TCP song song đói băng thông; voice không trễ quá 150 ms.
- Đổi mạng: khôi phục dưới 2 giây, không mất dữ liệu trên luồng tin cậy.

## Phụ thuộc dự kiến

usrsctp (BSD-3), KCP (MIT), thư viện Noise (mục 12 của thiết kế). Sẽ thêm vào `vcpkg.json` khi bắt đầu viết.
