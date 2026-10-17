# Bảo mật

Dự án mã hóa thư viện phim và kênh trò chuyện của gia đình, nên lỗ hổng cần được báo riêng để có thời gian sửa trước khi công khai.

## Báo lỗ hổng

- Dùng tính năng **Report a vulnerability** (GitHub private vulnerability reporting) trong tab Security của repo.
- Không mở issue công khai, không gửi PR có mô tả cách khai thác trước khi lỗi được sửa.
- Nêu: phiên bản hoặc commit, thành phần (định dạng mã hóa, kênh Noise, ranh giới tiến trình, HAL...), cách tái hiện, và ảnh hưởng.

Maintainer sẽ phản hồi trong vòng 7 ngày, và thống nhất với người báo thời điểm công khai sau khi có bản sửa.

## Phạm vi

Trong phạm vi là mọi thứ thiết kế mục 13.1 hứa bảo vệ, ví dụ:

- Đọc được media, metadata, chat hay voice khi không có quyền.
- Lấy được khóa nội dung từ `media_api` hoặc từ đĩa, bản sao lưu.
- Sửa, tráo hoặc cắt cụt đơn vị mã hóa mà không bị phát hiện.
- Kết nối được vào server, hoặc nhận ra dịch vụ, khi không có thiết bị đã ghép.

Ngoài phạm vi là các trường hợp thiết kế mục 13.2 đã nêu, ví dụ máy server đang chạy bị chiếm quyền admin, hoặc thiết bị đã được cấp quyền bị malware kiểm soát.

Dự án hiện chưa có bản phát hành; mọi báo cáo áp dụng cho nhánh `main`.
