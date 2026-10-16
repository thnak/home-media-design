# B2 — Bậc rendition: bitrate, chất lượng, tốc độ

Encode từng bậc của thang rendition (thiết kế mục 5.3) ở vài mức bitrate bằng NVENC, đo:

- **Tốc độ**: bội số thời gian thực (1,0 = vừa kịp phát). Phiên trực tiếp cần tối thiểu 1,2 (mục 5.4.5).
- **Chất lượng**: VMAF so với nguồn (chỉ có nghĩa khi nguồn và bậc cùng dải động; với nguồn HDR, xem B3).
- **Dung lượng**: MB mỗi giờ phim.

## Yêu cầu

- FFmpeg bản có `hevc_nvenc`, `h264_nvenc`, `av1_nvenc` (RTX 40 trở lên), `scale_cuda` và `libvmaf`. Bản "full" của gyan.dev trên Windows có đủ.
- Driver NVIDIA mới.

## Chạy

```powershell
.\bench\transcode\run_ladder.ps1 -Source bench\samples\sdr_1080p.mkv -Machine desktop-rtx4070
```

Kết quả: `bench/results/B2_<máy>_<ngày>.csv` và các file encode trong `bench/results/B2_work/` (xóa sau khi xem).

Script là bản khởi đầu, chưa chạy thử trên máy thật; nếu FFmpeg báo lỗi tham số (thường gặp với `scale_cuda` hoặc `av1_nvenc` trên bản cũ), sửa trong mảng `$Rungs`.

## Đọc kết quả

Với mỗi bậc, chọn bitrate thấp nhất mà VMAF đạt ngưỡng (đề xuất khởi điểm: 93 cho 1080p, 90 cho 720p và 480p). Ghi bitrate đã chọn và tốc độ vào thiết kế mục 5.4.6.
