# B3 — Tone-map HDR sang SDR và Dolby Vision profile 5

Mục tiêu (thiết kế mục 5.4.6 và 5.5.1):

1. Chọn phương án tone-map cho nhánh SDR: libplacebo (Vulkan) hay `zscale` + `tonemap` (CPU).
2. Xác nhận libplacebo áp RPU của Dolby Vision profile 5 cho màu đúng (không ám xanh hoặc tím).
3. Đo tốc độ tone-map ở 1080p, để biết nhánh SDR có theo kịp thời gian thực không.

## Chạy

```powershell
.\bench\tonemap_dv\run_tonemap.ps1 -Source bench\samples\dv_p5.mkv -Times 60,300,900
```

Script xuất ảnh PNG ở các mốc thời gian cho từng phương án vào `bench/results/B3_<tên file>/`, cùng một bản "không xử lý" để thấy lỗi màu khi phát DV5 như HDR thường, và in tốc độ tone-map 60 giây phim.

Yêu cầu FFmpeg có `libplacebo`, `zscale` (libzimg) và hỗ trợ Vulkan; bản "full" của gyan.dev có đủ. Script là bản khởi đầu, chưa chạy thử trên máy thật.

## Đánh giá

So ảnh bằng mắt trên màn hình SDR đã hiệu chỉnh: da người, bầu trời, cảnh tối, đèn sáng. Ghi phương án chọn và ảnh minh họa đáng chú ý vào thiết kế mục 16.
