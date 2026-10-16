# B4 — Prototype âm thanh: phát phim, voice, khử vọng, passthrough

Prototype C++ nhỏ trên Windows để kiểm chứng pipeline audio đề xuất (thiết kế mục 6.4 và 7.6) trước giai đoạn Watch Party. Chưa viết.

## Phạm vi

1. Giải mã track audio phim bằng FFmpeg, phát qua miniaudio (WASAPI chế độ chia sẻ).
2. Thu micro bằng miniaudio, đưa qua webrtc-audio-processing; tín hiệu tham chiếu là toàn bộ âm thanh đầu ra (phim + giọng giả lập).
3. Ghi lại tín hiệu micro sau khử vọng ra WAV để nghe và đo.
4. Ducking: giảm âm lượng phim khi phát hiện giọng nói.
5. Nhánh riêng: passthrough E-AC-3 JOC (Atmos) qua WASAPI chế độ độc quyền, đóng gói IEC 61937 bằng muxer `spdif` của FFmpeg, ra receiver hoặc soundbar.

## Tiêu chí đạt

- Phát phim bằng loa ngoài, nói vào micro: bản ghi sau khử vọng nghe rõ giọng, không còn tiếng phim đáng kể.
- Độ trễ đầu cuối mic → loa của máy khác (khi có mạng) dưới 200 ms; trong prototype đo độ trễ xử lý cục bộ.
- Receiver hiển thị Dolby Atmos khi passthrough.

## Phụ thuộc dự kiến

miniaudio (public domain/MIT-0), webrtc-audio-processing (BSD), libopus (BSD), FFmpeg (LGPL, link động). Sẽ thêm vào `vcpkg.json` khi bắt đầu viết.
