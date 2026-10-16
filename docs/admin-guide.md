# Hướng dẫn quản trị

Viết cho admin gia đình, không cần kiến thức mật mã (thiết kế mục 4.7). Tài liệu này sẽ được viết đầy đủ khi các tính năng tương ứng được triển khai; dưới đây là khung các mục bắt buộc.

## 1. Cài đặt lần đầu

- Yêu cầu phần cứng: CPU có AVX2, GPU NVIDIA (khuyến nghị RTX 40 trở lên nếu cần AV1 cho HDR).
- Tạo passphrase quản trị.
- Tạo và cất giữ bộ khôi phục (giấy hoặc QR). Ứng dụng yêu cầu xác nhận trước khi import phim đầu tiên.

## 2. Mở khóa sau khi khởi động lại

- Khi key store đang khóa: phim đã chia sẻ vẫn phát được; import và transcode phải chờ.
- Mở khóa từ ứng dụng admin hoặc tại máy server.
- Tùy chọn tự mở bằng TPM và đánh đổi của nó.

## 3. Sao lưu và chuyển máy

- Những gì cần sao lưu: thư mục dữ liệu ciphertext, database, bộ khôi phục.
- Khôi phục sang máy mới.

## 4. Thiết bị

- Duyệt thiết bị mới.
- Thu hồi thiết bị bị mất và giới hạn của thu hồi.

## 5. Khóa

- Đổi passphrase.
- Xoay vòng khóa của một asset.

## 6. Tối ưu hệ thống

- Bật quyền large page (Windows: SeLockMemoryPrivilege; Linux: transparent huge pages).
- Bật kTLS trên Linux.
- Kiểm tra trong log rằng các tối ưu đang hoạt động.

## 7. Không được làm

- Xóa hoặc làm mất bộ khôi phục.
- Bật tự mở khóa khi chưa hiểu đánh đổi.
- Để file phim gốc plaintext trong thư mục dữ liệu của server.
