# Đóng góp cho home-media

Cảm ơn bạn quan tâm. Dự án đang ở giai đoạn thiết kế và benchmark, và có ít phần cứng để thử, nên các đóng góp hữu ích nhất hiện nay là:

1. **Kiểm chứng phần cứng bạn có**, đặc biệt các loại ưu tiên cao trong [docs/hardware-matrix.md](docs/hardware-matrix.md): GPU NVIDIA Turing, Intel Quick Sync, server nhiều GPU, màn hình HDR, Linux. Kết quả được ghi vào [docs/verified-devices.csv](docs/verified-devices.csv) bằng PR.
2. **Góp ý thiết kế**: đọc [docs/design-v0.2.md](docs/design-v0.2.md), nhất là các mục còn mở ở mục 16.
3. **Viết mã** cho các phần đã chốt thiết kế: định dạng mã hóa, HAL, kênh truyền.

## Trước khi bắt đầu

- Với thay đổi lớn (thư viện mới, backend mới, đổi định dạng), mở issue trước để thống nhất hướng đi.
- Thiết kế chỉ duy trì trong repo. Thay đổi thiết kế đi cùng PR: sửa mục liên quan trong `docs/design-v0.2.md` và cập nhật mục 16 (đề xuất để `[ ]`, maintainer chốt thành `[x]`).

## Build và kiểm thử

Làm theo [README.md](README.md) (Windows 11 hoặc Linux, CMake 3.25+, Ninja, vcpkg). Trước khi gửi PR:

```powershell
cmake --preset windows-debug
cmake --build --preset windows-debug
ctest --preset windows-debug
```

Bước configure phải in `Boundary check OK`. Repo chưa có CI; người review sẽ chạy lại các bước này.

## Các quy tắc không được phá

Đây là các tính chất bảo mật của thiết kế. PR vi phạm sẽ không được nhận, kể cả khi có lý do hiệu năng.

- **Ranh giới khóa (mục 11).** `media_api`, `media_relay` và `hal_core` không được phụ thuộc, dù trực tiếp hay bắc cầu, vào `crypto_core`, `hal_media`, `hal_keyvault` hay bất kỳ target giữ khóa nào. Không lách bằng cách biên dịch thẳng file nguồn của các target đó.
- **Không có plaintext trên đĩa phía server (mục 13.3).** Không file tạm, không cache plaintext; FFmpeg chỉ dùng I/O tùy biến qua bộ nhớ.
- **Log không chứa tên phim, đường dẫn gốc hay khóa.** Chỉ ghi ID mờ.
- **Không tự viết mật mã hay giao thức bắt tay.** Dùng libsodium và thư viện Noise đã chọn (mục 12).
- **Đổi định dạng trên đĩa hoặc trên đường truyền** phải tăng `kFormatVersion` và cập nhật `docs/crypto-format.md`.

## Giấy phép

- Đóng góp của bạn được phát hành theo giấy phép MIT của dự án.
- Thư viện mới phải có giấy phép cho phép phân phối cùng dự án MIT (thiết kế mục 16): MIT, BSD, ISC, zlib, Apache-2.0, Boost, public domain dùng tự do; LGPL chỉ khi link động; MPL-2.0 chỉ khi không sửa file của thư viện. **Không dùng GPL, AGPL hay thành phần nonfree**, kể cả encoder phổ biến như x264, x265.
- Ghi thư viện mới vào bảng ở thiết kế mục 12 cùng giấy phép của nó.

## Thêm một backend HAL

Ví dụ backend Intel Quick Sync hay AMD AMF (thiết kế mục 11.3). Một backend được nhận khi:

1. Cài đặt interface của miền tương ứng trong `hal/`, có `probe()` trả về năng lực hoặc lý do không dùng được.
2. Không làm thay đổi hành vi của các backend khác khi phần cứng của nó không có mặt.
3. Qua bộ kiểm thử tuân thủ chung của miền: decode và encode phim mẫu chuẩn, keyframe đúng timeline segment (mục 5.4.2), VMAF đạt ngưỡng của bậc.
4. Có kết quả B2 (và B3 nếu backend làm tone-map) trên ít nhất một máy, đính kèm trong PR.
5. Có ít nhất một dòng `pass` với `retest` là `yes` trong `docs/verified-devices.csv` cho loại phần cứng của backend. Chưa có thì backend vẫn được nhận nhưng ở mức "dự kiến".

## Phong cách mã

- C++20, định dạng theo `.clang-format` và `.editorconfig`.
- Chú thích trong mã nguồn viết tiếng Việt không dấu hoặc tiếng Anh, giữ ngắn và tham chiếu mục thiết kế khi cần (ví dụ `// thiet ke muc 3.3`). Tài liệu trong `docs/` viết tiếng Việt có dấu.
- Commit message: một dòng tóm tắt, rồi danh sách thay đổi; tiếng Việt không dấu hoặc tiếng Anh.

## Báo lỗ hổng bảo mật

Không mở issue công khai cho lỗ hổng. Xem [SECURITY.md](SECURITY.md).
