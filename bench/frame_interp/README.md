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

`run_interp.ps1` chỉ đo ncnn Vulkan cho bậc ML, qua ảnh PNG. Việc đo các backend trên khung trong bộ nhớ GPU làm bằng `ort_rife.py` (ONNX Runtime): CUDA fp32, TensorRT fp32 và TensorRT fp16 trên cùng một mô hình, không qua PNG. Còn chưa có trong bộ bench:

- ONNX Runtime provider DirectML, và bản int8.
- Tự kiểm sau khi tối ưu theo ngưỡng của thiết kế (hiện chỉ in PSNR so với khung thật).
- Đo qua đường decode đến present thật của client (khung ở lại GPU suốt đường).

### `ort_rife.py`

Mô hình `rife_v4.6.onnx` của [vs-mlrt](https://github.com/AmusementClub/vs-mlrt) (gói `models.v15.16.7z`, thư mục `models/rife/`) có một đầu vào `input` 11 kênh, không giống hợp đồng ở mục 11.3.3. Script bọc nó: nhận `frame0`, `frame1`, `timestep`, rồi xếp 11 kênh theo thứ tự đã tìm bằng thử và kiểm bằng PSNR so với khung thật: `img0(3), img1(3), timestep(1), lưới(2), 2/(kích thước-1)(2)`. Kích thước được đệm lên bội số của 64. Một gói mô hình thật cho client sẽ bọc việc này bên trong artifact để biên ngoài đúng hợp đồng.

```powershell
$sp = "C:\Tools\ort-venv\Lib\site-packages"   # onnxruntime-gpu, numpy, tensorrt-cu13-libs<11, nvidia-cudnn-cu13
python -I bench\frame_interp\ort_rife.py --model rife_v4.6.onnx --source bench\samples\Meridian_UHD4k5994_HDR_P3PQ.mp4 `
    --machine ryzen9600x-rtx4060 --dll-dir "$sp\tensorrt_libs" --dll-dir "$sp\nvidia\cudnn\bin" --dll-dir "$sp\nvidia\cu13\bin"
```

Đo thời gian suy luận thuần (đầu vào đã ở GPU, p50 và p95 sau khởi động), thời gian nạp và dựng engine, và PSNR so với khung giữa thật trên 12 bộ ba. Kết quả: `bench/results/B6ort_<máy>_<ngày>.csv`. Nguồn Meridian là PQ P3 không gắn thẻ màu, đọc như RGB thường nên PSNR chỉ để so các backend và bậc với nhau, không phải số đo HDR.

Kết quả đã đo (RTX 4060, Ryzen 5 9600X, ORT 1.31, TensorRT 10, Meridian):

| Độ phân giải | Backend | Dựng/nạp | Suy luận p50 | PSNR (lặp / trộn / mô hình) |
| --- | --- | --- | --- | --- |
| 1080p | CUDA fp32 | 0,2 s | 52,1 ms | 33,5 / 39,2 / 44,3 |
| 1080p | TensorRT fp32 | 48 s | 31,5 ms | 44,3 |
| 1080p | TensorRT fp16 | 81 s | 12,9 ms | 44,0 |
| 4K | CUDA fp32 | 0,1 s | 215,9 ms | 33,4 / 39,0 / 44,4 |
| 4K | TensorRT fp32 | 102 s | 128,9 ms | 44,4 |
| 4K | TensorRT fp16 | 209 s | 53,3 ms | 44,0 |

Đọc nhanh: TensorRT fp16 nhanh gấp 4 lần CUDA fp32 và chỉ mất khoảng 0,3 dB, không khung nào lỗi. Nội suy 24 → 60 fps cần 36 lần suy luận mỗi giây: ở 1080p fp16 là khoảng 46% thời gian GPU (còn dư cho decode và màu, sát ngưỡng 50% của thiết kế); ở 4K fp16 một khung 53 ms nên chỉ đạt khoảng 19 khung/s, không đủ. 4K cần giảm độ phân giải tính toán (mục "Tối ưu" của thiết kế) hoặc mô hình nhẹ hơn. Dựng engine mất 1 đến 3,5 phút, nên cần chạy nền và dùng bậc thấp hơn trong lúc chờ như thiết kế đã nêu.

### `mci_vk/`: bậc 2 bằng compute shader Vulkan

Nguyên mẫu bậc 2 (mục 11.3.3) viết bằng compute shader, không dùng thư viện ngoài: ước lượng chuyển động theo khối 16×16 bằng tìm toàn phần (`motion.comp`), rồi dựng khung giữa bằng cách nội suy song tuyến vector giữa tâm các khối kề nhau và lấy mẫu hai khung (`synth.comp`). Vùng có sai số bù chuyển động lớn hơn sai số trộn thẳng thì dùng trộn thẳng. Đo thời gian GPU bằng timestamp query và PSNR so với khung giữa thật, cùng bộ ba khung với `ort_rife.py` để so được.

```powershell
glslc bench\frame_interp\mci_vk\motion.comp -o motion.spv
glslc bench\frame_interp\mci_vk\synth.comp  -o synth.spv
cl /O2 /std:c++20 /EHsc /I"$env:VULKAN_SDK\Include" bench\frame_interp\mci_vk\mci_bench.cpp /link "$env:VULKAN_SDK\Lib\vulkan-1.lib"
ffmpeg -ss 10 -i bench\samples\Meridian_UHD4k5994_HDR_P3PQ.mp4 -frames:v 25 -vf scale=1920:1080 -f rawvideo -pix_fmt rgb24 m1080.raw
mci_bench m1080.raw 1920 1080 12 motion.spv synth.spv <range> <lambda> <margin>
```

`range` là bán kính tìm theo điểm ảnh (nửa vector), `lambda` phạt vector dài, `margin` là ngưỡng quay về trộn thẳng. Đây là nguyên mẫu, chưa phân tầng, chưa kiểm tra lỗi biên: giá trị đo dưới đây dùng để chọn hướng, không phải để chốt số cuối.

Kết quả (RTX 4060, 12 bộ ba, PSNR dB; thời gian GPU của shader chỉ là ước lượng, vì xung GPU dao động đến 2 lần giữa các lần chạy):

| Nguồn | Lặp | Trộn | Shader (range 4 / 8 / 16) | RIFE v4.6 TensorRT fp16 |
| --- | --- | --- | --- | --- |
| Meridian 1080p, ít chuyển động | 33,5 | 39,1 | 44,6 / 43,9 / 38,4 | 44,0 |
| Meridian 4K | 33,4 | 38,9 | — / 43,7 / 41,1 | 44,0 |
| Big Buck Bunny 1080p, giây 120, chuyển động nhanh | 25,4 | 28,0 | 28,5 / 28,8 / 29,0 | 34,2 |
| Big Buck Bunny 1080p, giây 400, cảnh yên | 37,0 | 42,3 | 45,5 / 45,4 / 43,1 | 43,9 |

Thời gian: shader ở 1080p mất 1,7 ms (range 4), 4,4 ms (range 8), khoảng 15 ms (range 16); ở 4K range 8 mất khoảng 77 ms. RIFE fp16 mất 13 ms ở 1080p và 53 ms ở 4K.

Kết luận từ số đo:

- Bù chuyển động bằng khối ngang RIFE khi chuyển động nhỏ và rẻ hơn nhiều (1,7 ms so với 13 ms), nhưng khi chuyển động nhanh nó chỉ hơn trộn thẳng khoảng 1 dB còn RIFE hơn 6 dB. Đây đúng là chỗ bậc 3 có giá trị, nên tách bậc 2 và bậc 3 như thiết kế là hợp lý.
- Tăng `range` làm kết quả tệ đi ở cả hai nguồn vì tìm toàn phần gặp nhiều kết quả khớp sai. Muốn bắt chuyển động lớn phải làm tìm theo tầng (kim tự tháp độ phân giải) và làm mượt vector, không phải tăng bán kính; chưa làm trong nguyên mẫu.
- Mẫu cho 24 fps có khoảng cách khung lớn hơn 60 fps nên chuyển động giữa hai khung nguồn lớn hơn các mẫu Meridian ở đây; số đo ít chuyển động không đại diện cho phim.
- Ở 4K, ước lượng chuyển động toàn phần mất khoảng 77 ms (range 8), quá chậm cho thời gian thực; cần tìm theo tầng hoặc ước lượng ở độ phân giải thấp hơn rồi phóng vector.
- Cả hai bậc đều có khung PSNR thấp (min khoảng 12 đến 15 dB) ở cửa sổ có đổi cảnh; phát hiện đổi cảnh trong thiết kế cần để quay về lặp khung ở đó.

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
