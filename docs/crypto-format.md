# Định dạng mã hóa — bản nháp v1

Đặc tả định dạng đơn vị mã hóa như đang triển khai trong `shared/` và `crypto/`. Bám theo thiết kế mục 3.2–3.4 và 4.4. Đây là bản nháp: định dạng có thể đổi cho tới khi benchmark B1 xong và `kFormatVersion` được đóng băng.

## Thuật toán

| Thành phần | Lựa chọn |
| --- | --- |
| AEAD | XChaCha20-Poly1305 IETF (libsodium `crypto_aead_xchacha20poly1305_ietf_*`) |
| Khóa | 256 bit |
| Nonce | 192 bit, sinh ngẫu nhiên cho mỗi lần mã hóa |
| Tag | 128 bit |
| Khóa con theo variant | `crypto_kdf_derive_from_key(subkey, 32, variant_id, "hmvarkey", DEK)` |

Nonce ngẫu nhiên 192 bit đủ lớn để không lo trùng lặp, nên không cần bộ đếm nonce lưu trạng thái. Mã hóa lại một đơn vị luôn sinh nonce mới.

## Một đơn vị trên đĩa

```
offset  kích thước  nội dung
0       24          nonce
24      N           ciphertext (N = plaintext_len)
24+N    16          tag Poly1305
```

Phần phụ trội cố định 40 byte mỗi đơn vị (`kUnitOverhead`).

## Associated Data: UnitId

Mỗi đơn vị được ràng buộc với vị trí của nó bằng AAD 39 byte, little-endian:

| Offset | Kích thước | Trường | Ghi chú |
| --- | --- | --- | --- |
| 0 | 2 | `format_version` | Hiện là 1 |
| 2 | 1 | `kind` | 1 original chunk, 2 media segment, 3 init segment, 4 manifest |
| 3 | 16 | `asset_id` | ID mờ của asset |
| 19 | 4 | `generation` | Đổi khi mã hóa lại asset |
| 23 | 4 | `variant_id` | 0 = original |
| 27 | 8 | `unit_index` | Vị trí trong variant |
| 35 | 4 | `plaintext_len` | Phải bằng độ dài thực của plaintext |

Mở đơn vị với `UnitId` khác bất kỳ trường nào sẽ thất bại. Nhờ vậy phát hiện được việc tráo đơn vị giữa asset, generation, variant hoặc vị trí, và việc cắt cụt dữ liệu. `tests/unit/test_unit_aead.cpp` kiểm tra từng trường hợp này.

## Khóa trong bộ nhớ

`SecretKey` cấp phát bằng `sodium_malloc`: vùng nhớ được khóa (không bị swap), có trang bảo vệ hai bên và được xóa sạch khi hủy (thiết kế mục 13.3).

## Chưa đặc tả

- Manifest: cấu trúc, cách mã hóa (kind = 4), kiểm tra giới hạn.
- Bọc DEK bằng KEK và envelope cho thiết bị.
- Key store: KEK từ passphrase (Argon2id), TPM, bộ khôi phục.
- Kích thước đơn vị lưu trữ mặc định cho original (chờ B1).
