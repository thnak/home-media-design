# Bảng máy thử phần cứng

Dự án có ít phần cứng, nên phần lớn việc thử trên GPU, màn hình HDR và thiết bị âm thanh dựa vào người đóng góp. Bảng này ghi phần cứng nào đang có người thử, để biết dòng nào trong thiết kế mục 11.2 thực sự được hỗ trợ chính thức.

Quy tắc (thiết kế mục 16): một loại phần cứng chỉ được ghi là hỗ trợ chính thức khi có ít nhất một người thử ở bảng dưới, và người đó chạy lại kiểm thử trước mỗi bản phát hành.

## Cách đóng góp kết quả

1. Build theo `README.md`, chạy các benchmark phù hợp với phần cứng của bạn (`bench/README.md`) trên bộ phim mẫu chuẩn ở cuối trang này.
2. Mở issue theo mẫu **Báo cáo phần cứng**: cấu hình máy, phiên bản driver, bản FFmpeg, file CSV kết quả đính kèm.
3. Nếu bạn nhận giữ máy thử lâu dài, gửi PR thêm hoặc sửa dòng của mình trong bảng dưới.

Không đưa file phim lên repo hay issue. File kết quả thô cũng không đưa vào git (`bench/results/README.md`); đính kèm vào issue.

## Server: GPU transcode

| Phần cứng | Thế hệ | Backend | Năng lực đáng chú ý | Người thử | Trạng thái |
| --- | --- | --- | --- | --- | --- |
| NVIDIA RTX 40 | Ada | NVENC, NVDEC | Encode H.264, HEVC, AV1; decode AV1 | | Cần người thử |
| NVIDIA RTX 20, GTX 16 | Turing | NVENC, NVDEC | Không encode AV1; mức thấp nhất được hỗ trợ | | **Cần người thử** |
| NVIDIA RTX 30 | Ampere | NVENC, NVDEC | Không encode AV1, decode AV1 | | Cần người thử |
| NVIDIA RTX 50 | Blackwell | NVENC, NVDEC | Encode AV1, nhiều phiên hơn | | Cần người thử |
| Intel iGPU (UHD, Iris Xe, N100/N150) | Gen 11–12 | Quick Sync | Mini PC làm server gia đình thường dùng loại này | | **Cần người thử** (backend thứ hai) |
| Intel Arc (A, B series), Core Ultra iGPU | Xe-HPG, Xe-LPG trở lên | Quick Sync | Encode AV1 | | **Cần người thử** |
| AMD RDNA1, RDNA2 (RX 5000, RX 6000, bản desktop và laptop) | VCN 2.0–3.0 | AMF, VAAPI | Encode H.264, HEVC; không AV1 | | Cần người thử (backend thứ ba) |
| AMD RX 7000 trở lên | RDNA3 trở lên | AMF, VAAPI | Encode AV1 | | Cần người thử |
| Server nhiều GPU (cùng hãng hoặc khác hãng) | | Mọi backend | Lập lịch và phân bổ theo từng GPU (thiết kế mục 5.4.9) | | **Cần người thử** |
| Không GPU, CPU AVX2 | | CPU (OpenH264, Kvazaar, SVT-AV1) | Đường lùi khi không có GPU | | Cần người thử |
| CPU có AVX-512 | | CPU | Kiểm tra libsodium và SVT-AV1 chọn đúng nhánh | | Cần người thử |

## Server: hệ điều hành và phần cứng khác

| Hạng mục | Cần thử | Người thử | Trạng thái |
| --- | --- | --- | --- |
| Windows 11 x64 | Toàn bộ server | | Cần người thử |
| Linux x86-64 (Ubuntu LTS, Debian) | Toàn bộ server, io_uring, large page | | Cần người thử |
| TPM 2.0 trên Linux (tpm2-tss) | Niêm phong KEK | | Cần người thử |
| Router có UPnP, NAT-PMP hoặc PCP | Mở cổng (thiết kế mục 9.5) | | Cần người thử, ghi rõ hãng và model router |
| Đường truyền CGNAT, IPv6 | Relay, IPv6 trực tiếp (mục 9.7) | | Cần người thử, ghi rõ nhà mạng |

## Client Windows 11

| Hạng mục | Cần thử | Người thử | Trạng thái |
| --- | --- | --- | --- |
| D3D11VA trên NVIDIA | Decode và chia sẻ texture sang Vulkan | | Cần người thử |
| D3D11VA trên AMD | Như trên | | Cần người thử |
| Laptop đồ họa lai (iGPU + GPU rời) | Chọn đúng adapter cho decode và hiển thị (thiết kế mục 11.3.1) | | Cần người thử |
| D3D11VA trên Intel iGPU | Như trên | | Cần người thử |
| Màn hình HDR10 | Nhận diện HDR qua DXGI, swapchain HDR10 | | Cần người thử |
| Receiver hoặc soundbar có Dolby Atmos | Passthrough E-AC-3 JOC, TrueHD (B4) | | Cần người thử |
| TPM 2.0 qua CNG | Bọc khóa thiết bị | | Cần người thử |
| Windows 11 ARM64 | Build và phát cơ bản | | Dự kiến, chưa cần |

## Bộ phim mẫu chuẩn

Kết quả giữa các máy chỉ so được khi chạy trên cùng nguồn. Bộ mẫu chuẩn dùng nội dung có giấy phép mở, ai cũng tải được:

| Vai trò | Nguồn gợi ý | Giấy phép |
| --- | --- | --- |
| SDR 1080p H.264 | Big Buck Bunny hoặc Sintel (Blender Foundation) | CC BY |
| SDR 4K | Tears of Steel 4K (Blender Foundation) | CC BY |
| HDR10 2160p | Netflix Open Content (ví dụ Meridian, Sol Levante) | CC BY 4.0 |

Kiểm tra lại giấy phép tại trang của từng nguồn trước khi dùng. Mẫu HLG, Dolby Vision profile 5 và 8.1, và track Atmos hiện chưa có nguồn mở phù hợp: dùng file của riêng bạn, chỉ báo kết luận và ảnh so sánh, không gửi file.
