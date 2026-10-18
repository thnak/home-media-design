# Benchmark trước khi chốt thiết kế

Các quyết định còn mở trong thiết kế (mục 16) được chốt bằng số đo, không bằng phỏng đoán. Mỗi benchmark ghi kết quả vào `bench/results/` theo tên `<mã>_<máy>_<ngày>.csv` để so sánh giữa các máy.

| Mã | Câu hỏi cần trả lời | Mục thiết kế | Công cụ | Trạng thái |
| --- | --- | --- | --- | --- |
| B1 | Kích thước đơn vị lưu trữ, số đơn vị gộp mỗi request | 3.2 | `bench_crypto_chunk` (C++) | Chạy được |
| B2 | Bitrate từng bậc rendition, tốc độ NVENC so với thời gian thực | 5.3, 5.4.5, 5.4.6 | `transcode/run_ladder.ps1` (FFmpeg + VMAF) | Script khởi đầu |
| B3 | Tone-map HDR sang SDR, Dolby Vision profile 5 | 5.5.1 | `tonemap_dv/run_tonemap.ps1` (FFmpeg + libplacebo) | Script khởi đầu |
| B4 | Phát phim + thu micro + khử vọng; passthrough Atmos | 6.4, 7.6, 5.5.2 | `audio_aec/` (prototype C++) | Chưa viết |
| B5 | SCTP (usrsctp) trên Noise có đạt thông lượng, độ trễ và công bằng không; so với KCP | 9.8 | `transport/` (prototype C++) | Chưa viết |
| B6 | Các bậc nội suy khung hình phía client: tốc độ, VMAF, bậc nào vào bản đầu | 11.3.3 | `frame_interp/run_interp.ps1` (FFmpeg, VMAF, rife-ncnn-vulkan) | Script khởi đầu |

Thứ tự đề xuất: B1 (vài phút) → B2 và B3 (cần GPU và phim mẫu) → B4 và B5 (prototype riêng); B6 độc lập, cần nguồn 60 fps.

## Phim mẫu

Đặt phim mẫu vào `bench/samples/` (đã nằm trong `.gitignore`, không đưa lên repo). Bộ tối thiểu:

- 1 phim SDR 1080p H.264, khoảng 10 phút, nhiều chuyển động.
- 1 phim HDR10 2160p HEVC.
- 1 phim HLG (nếu có, ví dụ quay từ điện thoại).
- 1 phim Dolby Vision profile 5, 1 phim profile 8.1.
- 1 file có track E-AC-3 JOC (Atmos) cho B4.

Kiểm tra profile Dolby Vision bằng `ffprobe -v error -show_streams -select_streams v:0 <file>` và tìm `dv_profile` trong `side_data`.

## Ghi kết quả

Ghi kèm cấu hình máy ở đầu file hoặc tên file: CPU, GPU, phiên bản driver NVIDIA, phiên bản FFmpeg. Kết luận cho từng benchmark được cập nhật vào thiết kế, mục 16.
