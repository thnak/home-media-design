// Ma hoa xac thuc tung don vi (chunk/segment) bang XChaCha20-Poly1305.
// Thiet ke muc 3.2–3.3 va 4.4. Dinh dang 1 don vi tren dia:
//   nonce (24 byte) | ciphertext | tag (16 byte)
// Associated Data = hm::protocol::serialize(UnitId), nen tra mot don vi sang
// asset, generation, variant hoac vi tri khac se bi tu choi khi mo.
#pragma once

#include "hm/protocol/unit_id.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace hm::crypto {

inline constexpr std::size_t kKeySize = 32;
inline constexpr std::size_t kNonceSize = 24;
inline constexpr std::size_t kTagSize = 16;
inline constexpr std::size_t kUnitOverhead = kNonceSize + kTagSize;

// Phai goi mot lan truoc khi dung bat ky ham nao khac. An toan khi goi nhieu lan.
bool initialize() noexcept;

// Khoa duoc khoa trang nho (khong bi swap) va xoa sach khi huy (thiet ke muc 13.3).
class SecretKey {
public:
    SecretKey();                       // tao ngau nhien
    ~SecretKey();
    SecretKey(const SecretKey&) = delete;
    SecretKey& operator=(const SecretKey&) = delete;
    SecretKey(SecretKey&&) noexcept;
    SecretKey& operator=(SecretKey&&) noexcept;

    // Khoa con cho mot variant, dan xuat tu DEK cua generation (thiet ke muc 4.4).
    SecretKey derive_variant_key(std::uint32_t variant_id) const;

    std::span<const std::uint8_t, kKeySize> bytes() const noexcept;

private:
    struct Uninitialized {};
    explicit SecretKey(Uninitialized);
    std::uint8_t* data_ = nullptr;
};

// Ma hoa plaintext thanh mot don vi. id.plaintext_len phai bang plaintext.size().
std::vector<std::uint8_t> seal_unit(const SecretKey& key, const protocol::UnitId& id,
                                    std::span<const std::uint8_t> plaintext);

// Ghi vao bo dem co san, tranh cap phat khi chay lien tuc (benchmark, pipeline).
// out.size() phai >= plaintext.size() + kUnitOverhead. Tra ve so byte da ghi.
std::size_t seal_unit_into(const SecretKey& key, const protocol::UnitId& id,
                           std::span<const std::uint8_t> plaintext, std::span<std::uint8_t> out);

// Mo mot don vi. Tra ve false neu xac thuc that bai hoac kich thuoc sai;
// khi do out khong duoc dung.
bool open_unit_into(const SecretKey& key, const protocol::UnitId& id,
                    std::span<const std::uint8_t> unit, std::span<std::uint8_t> out) noexcept;

} // namespace hm::crypto
