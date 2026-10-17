# Phần cứng cần kiểm chứng

Dự án có ít phần cứng, nên phần lớn việc thử trên GPU, màn hình HDR và thiết bị âm thanh dựa vào người đóng góp. Có hai file:

- **Trang này**: các **loại** phần cứng mà thiết kế mục 11.2 muốn hỗ trợ, mỗi loại có một mã `class_id`. Trang này chỉ đổi khi thiết kế đổi.
- **[verified-devices.csv](verified-devices.csv)**: các **thiết bị cụ thể** đã được kiểm chứng, mỗi lần kiểm chứng một dòng. Contributor thêm hoặc cập nhật dòng của mình bằng PR.

Quy tắc (thiết kế mục 16): một loại phần cứng được coi là **hỗ trợ chính thức** khi `verified-devices.csv` có ít nhất một dòng cùng `class_id` thỏa cả ba điều kiện:

- `result` là `pass`.
- `retest` là `yes`, nghĩa là người đó sẵn sàng chạy lại trước mỗi bản phát hành.
- `commit` thuộc chu kỳ phát hành hiện tại. Trước bản phát hành đầu tiên: bất kỳ commit nào.

Loại chưa có dòng nào như vậy là "dự kiến".

## Cách đóng góp

1. Build theo `README.md`, chạy các benchmark và kiểm thử phù hợp với phần cứng của bạn (`bench/README.md`) trên bộ phim mẫu chuẩn ở cuối trang này.
2. Mở issue theo mẫu **Báo cáo phần cứng**, đính kèm file CSV kết quả của benchmark, log và ảnh so sánh.
3. Gửi PR thêm một dòng vào `verified-devices.csv`, cột `evidence` trỏ tới issue ở bước 2. Maintainer kiểm tra bằng chứng rồi merge.
4. Khi chạy lại trên commit mới hoặc driver mới, thêm dòng mới thay vì sửa dòng cũ, để giữ lịch sử. Khi không còn giữ máy, đổi `retest` của các dòng của bạn thành `no`.

Không đưa file phim lên repo hay issue. File kết quả thô của benchmark cũng không đưa vào git (`bench/results/README.md`); đính kèm vào issue.

## Các cột của verified-devices.csv

| Cột | Nội dung | Ví dụ |
| --- | --- | --- |
| `class_id` | Mã loại ở các bảng dưới | `gpu-nv-turing` |
| `vendor` | Hãng | `NVIDIA` |
| `model` | Tên thiết bị cụ thể | `GeForce RTX 2060` |
| `os` | Hệ điều hành và bản | `Windows 11 24H2` |
| `driver` | Phiên bản driver, firmware hoặc kernel liên quan | `581.57` |
| `backend` | Backend HAL đã thử | `nvenc` |
| `tests` | Benchmark và kiểm thử đã chạy, cách nhau bằng `;` | `B2;B3;ctest` |
| `result` | `pass`, `partial` (đạt một phần, ghi rõ trong issue) hoặc `fail` | `pass` |
| `commit` | Commit của repo đã build (ít nhất 7 ký tự) | `b7296f1` |
| `date` | Ngày chạy, `YYYY-MM-DD` | `2026-10-20` |
| `evidence` | Số issue chứa kết quả | `#12` |
| `contributor` | Tên GitHub của người thử | `@ten-cua-ban` |
| `retest` | `yes` nếu sẵn sàng chạy lại trước bản phát hành, ngược lại `no` | `yes` |

Ghi giá trị có dấu phẩy trong ngoặc kép. Kết quả `fail` vẫn nên ghi lại: nó cho biết phần cứng hoặc driver nào đang có vấn đề.

Tên GitHub là bắt buộc để maintainer liên hệ khi cần chạy lại. Ghi tên vào file là tự nguyện; nếu không muốn công khai, chỉ cần mở issue ở bước 2 mà không gửi PR.

## Server: GPU transcode

| `class_id` | Loại | Backend | Năng lực đáng chú ý | Ưu tiên |
| --- | --- | --- | --- | --- |
| `gpu-nv-turing` | NVIDIA RTX 20, GTX 16 (Turing) | NVENC, NVDEC | Không encode AV1; mức thấp nhất được hỗ trợ | **Cao** |
| `gpu-nv-ampere` | NVIDIA RTX 30 (Ampere) | NVENC, NVDEC | Không encode AV1, decode AV1 | |
| `gpu-nv-ada` | NVIDIA RTX 40 (Ada) | NVENC, NVDEC | Encode H.264, HEVC, AV1; decode AV1 | |
| `gpu-nv-blackwell` | NVIDIA RTX 50 (Blackwell) | NVENC, NVDEC | Encode AV1, nhiều phiên hơn | |
| `gpu-intel-igpu` | Intel iGPU UHD, Iris Xe, N100/N150 (Gen 11–12) | Quick Sync | Mini PC làm server gia đình thường dùng loại này | **Cao** (backend thứ hai) |
| `gpu-intel-arc` | Intel Arc A, B series; iGPU Core Ultra | Quick Sync | Encode AV1 | **Cao** |
| `gpu-amd-rdna12` | AMD RX 5000, RX 6000, bản desktop và laptop (RDNA1, RDNA2) | AMF, VAAPI | Encode H.264, HEVC; không AV1 | Backend thứ ba |
| `gpu-amd-rdna3` | AMD RX 7000 trở lên (RDNA3 trở lên) | AMF, VAAPI | Encode AV1 | |
| `gpu-multi` | Server nhiều GPU, cùng hãng hoặc khác hãng | Mọi backend | Lập lịch theo từng GPU (thiết kế mục 5.4.9) | **Cao** |
| `cpu-avx2` | Không GPU, CPU AVX2 | CPU (OpenH264, Kvazaar, SVT-AV1) | Đường lùi khi không có GPU | |
| `cpu-avx512` | CPU có AVX-512 | CPU | libsodium và SVT-AV1 chọn đúng nhánh | |

## Server: hệ điều hành, mạng và phần cứng khác

| `class_id` | Loại | Cần thử | Ưu tiên |
| --- | --- | --- | --- |
| `os-win11` | Windows 11 x64 | Toàn bộ server | |
| `os-linux` | Linux x86-64 (Ubuntu LTS, Debian) | Toàn bộ server, io_uring, large page | **Cao** |
| `tpm-linux` | TPM 2.0 trên Linux (tpm2-tss) | Niêm phong KEK | |
| `net-portmap` | Router có UPnP, NAT-PMP hoặc PCP | Mở cổng (thiết kế mục 9.5); ghi hãng và model router vào `model` | |
| `net-cgnat` | Đường truyền CGNAT, IPv6 | Relay, IPv6 trực tiếp (thiết kế mục 9.7); ghi nhà mạng vào `vendor` | |

## Client Windows 11

| `class_id` | Loại | Cần thử | Ưu tiên |
| --- | --- | --- | --- |
| `cli-d3d11-nv` | D3D11VA trên NVIDIA | Decode và chia sẻ texture sang Vulkan | |
| `cli-d3d11-amd` | D3D11VA trên AMD | Như trên | |
| `cli-d3d11-intel` | D3D11VA trên Intel iGPU | Như trên | |
| `cli-hybrid` | Laptop đồ họa lai (iGPU + GPU rời) | Chọn đúng adapter cho decode và hiển thị (thiết kế mục 11.3.1) | |
| `cli-hdr10` | Màn hình HDR10 | Nhận diện HDR qua DXGI, swapchain HDR10 | **Cao** |
| `cli-atmos` | Receiver hoặc soundbar có Dolby Atmos | Passthrough E-AC-3 JOC, TrueHD (B4) | |
| `cli-tpm` | TPM 2.0 qua CNG | Bọc khóa thiết bị | |
| `cli-arm64` | Windows 11 ARM64 | Build và phát cơ bản | Dự kiến, chưa cần |

Cần thêm loại mới (ví dụ một dòng GPU chưa có ở đây): gửi PR sửa trang này, kèm lý do trong thiết kế.

## Bộ phim mẫu chuẩn

Kết quả giữa các máy chỉ so được khi chạy trên cùng nguồn. Bộ mẫu chuẩn dùng nội dung có giấy phép mở, ai cũng tải được:

| Vai trò | Nguồn gợi ý | Giấy phép |
| --- | --- | --- |
| SDR 1080p H.264 | Big Buck Bunny hoặc Sintel (Blender Foundation) | CC BY |
| SDR 4K | Tears of Steel 4K (Blender Foundation) | CC BY |
| HDR10 2160p | Netflix Open Content (ví dụ Meridian, Sol Levante) | CC BY 4.0 |

Kiểm tra lại giấy phép tại trang của từng nguồn trước khi dùng. Mẫu HLG, Dolby Vision profile 5 và 8.1, và track Atmos hiện chưa có nguồn mở phù hợp: dùng file của riêng bạn, chỉ báo kết luận và ảnh so sánh, không gửi file.
