# home-media

Media server gia đình mã nguồn mở bằng C++20: thư viện phim được mã hóa khi lưu và khi truyền, transcode bằng GPU NVIDIA theo năng lực từng thiết bị, Watch Party tối đa 15 người với chat và voice mã hóa đầu cuối.

**Trạng thái:** giai đoạn benchmark. Chưa có chức năng cho người dùng; mã hiện tại gồm khung build, lõi mã hóa đơn vị và các benchmark để chốt thiết kế. Chưa khởi tạo git cho tới khi benchmark rõ ràng.

Thiết kế: [docs/design-v0.2.md](docs/design-v0.2.md) (bản duy nhất, duy trì trong repo).

## Cấu trúc

```
home-media/
├── cmake/            CompilerOptions, BoundaryCheck (kiểm tra ranh giới bảo mật)
├── shared/           shared_protocol: định dạng wire, UnitId — không giữ khóa
├── crypto/           crypto_core: AEAD theo đơn vị, khóa con theo variant — giữ khóa
├── server/
│   ├── api/          media_api: kênh Noise, RPC, relay, SFU — không giữ khóa
│   └── worker/       media_worker: import, transcode, key store — giữ khóa
├── client/           media_client: ứng dụng Windows
├── tests/            test đơn vị, chạy qua CTest
├── bench/            benchmark B1–B5 và kết quả
└── docs/             thiết kế, định dạng mã hóa, hướng dẫn quản trị
```

Nếu `media_api` link tới `crypto_core`, dù trực tiếp hay bắc cầu, bước configure của CMake sẽ dừng với lỗi "Vi pham ranh gioi bao mat" (thiết kế mục 11).

## Build trên Windows

Yêu cầu: Visual Studio 2022 (workload C++), CMake 3.25+, Ninja, vcpkg.

```powershell
# Một lần: trỏ tới vcpkg có sẵn
setx VCPKG_ROOT C:\Users\thanh\Git\vcpkg
# Mở "x64 Native Tools Command Prompt for VS 2022" hoặc Developer PowerShell, rồi:
cd C:\Users\thanh\Git\home-media
cmake --preset windows-release
cmake --build --preset windows-release
ctest --test-dir build\windows-release --output-on-failure
```

vcpkg tự cài libsodium từ `vcpkg.json` ở lần configure đầu.

## Build trên Linux

```bash
export VCPKG_ROOT=~/vcpkg
cmake --preset linux-release
cmake --build --preset linux-release
ctest --test-dir build/linux-release --output-on-failure
```

## Benchmark

Xem [bench/README.md](bench/README.md). Bắt đầu bằng B1:

```powershell
.\build\windows-release\bench\bench_crypto_chunk.exe 4096
```

## Đóng góp

Dự án cần người thử trên phần cứng khác nhau (GPU NVIDIA Turing, Intel Quick Sync, AMD, màn hình HDR, receiver Atmos, Linux). Xem [CONTRIBUTING.md](CONTRIBUTING.md), [docs/hardware-matrix.md](docs/hardware-matrix.md) và danh sách thiết bị đã kiểm chứng [docs/verified-devices.csv](docs/verified-devices.csv). Báo lỗ hổng bảo mật theo [SECURITY.md](SECURITY.md).

## Giấy phép

MIT. Phụ thuộc phải tương thích khi phân phối: FFmpeg dùng bản LGPL link động, không kèm thành phần GPL hay nonfree (thiết kế mục 12).
