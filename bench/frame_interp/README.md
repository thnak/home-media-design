# B6 — Nội suy khung hình theo bậc

Đo các bậc nội suy khung hình phía client (thiết kế mục 11.3.3): từ pha trộn đến bù chuyển động cổ điển và ML. Câu hỏi cần trả lời:

1. Mỗi bậc chạy nhanh bao nhiêu lần thời gian thực ở 1080p và 4K trên từng loại GPU, để đặt ngưỡng "còn dư sức".
2. Chất lượng mỗi bậc so với bản gốc 60 fps (VMAF), và bậc nào đáng đưa vào bản đầu.
3. Với HDR, trộn trong miền tuyến tính hay miền PQ cho hình tốt hơn (làm sau khi có bậc 2 trên Vulkan).

## Phương pháp

Nguồn 24 fps không có "đáp án đúng" để so, nên dùng nguồn 60 fps: bỏ mỗi khung lẻ để có bản 30 fps, cho từng bậc dựng lại 60 fps, rồi so VMAF với đoạn 60 fps gốc. Đây là phép nội suy ×2 tiêu chuẩn; nó không đo trực tiếp 24 → 60, nhưng so được các bậc với nhau.

| Bậc | Công cụ trong script |
| --- | --- |
| `t0-repeat` | `fps` (lặp khung); mức sàn |
| `t1-blend` | `framerate` của FFmpeg |
| `t1-placebo` | `libplacebo` với `frame_mixer=linear` (Vulkan) |
| `t2-mci` | `minterpolate` (CPU, bù chuyển động); chậm, dùng làm mốc chất lượng cho shader sau này |
| `t3-rife` | [rife-ncnn-vulkan](https://github.com/nihui/rife-ncnn-vulkan) (MIT), mô hình `rife-v4.6` |

Script hiện chỉ đo ncnn Vulkan cho bậc ML. Thiết kế (mục 11.3.3) còn cần so các backend trên cùng một mô hình; phần này chưa có trong script:

- TensorRT (fp16, engine dựng sẵn) và ONNX Runtime (provider CUDA, DirectML) với cùng một mô hình ở nhiều artifact (ONNX fp32 và fp16, engine TensorRT, bản int8 nếu có) theo hợp đồng vào ra ở mục 11.3.3, đo thời gian suy luận thuần trên khung trong bộ nhớ GPU, không qua PNG.
- Cùng mô hình ở fp32 và fp16, và sau tối ưu so với bản chưa tối ưu để kiểm tra ngưỡng sai lệch.
- Đo thời gian dựng engine TensorRT và kích thước bộ đệm.

## Chạy

```powershell
.\bench\frame_interp\run_interp.ps1 -Source "bench\samples\Test Jellyfin 1080p AVC 5M.mp4" -Machine ryzen9600x-rtx4060 -Rife C:\Tools\rife\rife-ncnn-vulkan.exe
```

- Nguồn phải từ 50 fps trở lên. Mẫu 1080p60 trong `hardware-matrix.md` dùng được; `Meridian_UHD4k5994_HDR_P3PQ.mp4` (Netflix Open Content, 4K 59,94 fps) dùng cho 4K với `-Height 2160`.
- `-Seconds` mặc định 5. `t2-mci` ở 4K rất chậm; giữ đoạn ngắn.
- RIFE chạy qua ảnh PNG nên ở 4K bị chặn bởi đọc ghi đĩa; số đo bậc 3 chỉ tính thời gian gọi RIFE. Đo 4K cho bậc 3 cần prototype nhận khung trong bộ nhớ.

Kết quả: `bench/results/B6_<máy>_<ngày>.csv`, file tạm trong `bench/results/B6_work/` (xóa sau khi xem).

## Đọc kết quả

- Tốc độ thời gian thực là số lần nhanh hơn việc phát; còn lại phải đủ chỗ cho decode, xử lý màu và phụ đề (thiết kế đề xuất: dưới 50% khoảng cách giữa hai lần present).
- Tốc độ của bậc 0 đến 2 gồm cả decode và ghi ffv1, nên bậc 0 cho biết chi phí nền; hiệu giữa bậc và bậc 0 mới là chi phí nội suy.
- VMAF cao hơn bậc 0 và bậc 1 mới đáng kể. Kiểm tra bằng mắt ở cảnh chuyển động nhanh, chữ chạy, đổi cảnh; VMAF không bắt được lỗi viền và bóng ma.
- Ghi bậc nào chạy được ở 1080p và 4K trên từng GPU vào thiết kế mục 16.
