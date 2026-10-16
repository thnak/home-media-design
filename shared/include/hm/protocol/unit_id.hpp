// Dinh danh mot don vi ma hoa (chunk cua original hoac segment cua rendition).
// Thiet ke muc 3.2 va 3.3. Day la du lieu khong bi mat: server dung no de
// dinh vi file; client dung ban serialize lam Associated Data cho AEAD.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace hm::protocol {

// Tang khi dinh dang tren dia hoac tren day thay doi theo cach khong tuong thich.
inline constexpr std::uint16_t kFormatVersion = 1;

using AssetId = std::array<std::uint8_t, 16>;

enum class UnitKind : std::uint8_t {
    OriginalChunk = 1, // cat theo byte, thiet ke muc 6.1
    MediaSegment = 2,  // segment CMAF, thiet ke muc 6.2
    InitSegment = 3,
    Manifest = 4,
};

struct UnitId {
    std::uint16_t format_version = kFormatVersion;
    UnitKind kind = UnitKind::OriginalChunk;
    AssetId asset_id{};
    std::uint32_t generation = 0;
    std::uint32_t variant_id = 0;   // 0 = original
    std::uint64_t unit_index = 0;
    std::uint32_t plaintext_len = 0;
};

// Kich thuoc ban serialize co dinh, little-endian:
// 2 (version) + 1 (kind) + 16 (asset) + 4 (generation) + 4 (variant) + 8 (index) + 4 (len)
inline constexpr std::size_t kUnitIdWireSize = 39;

using UnitIdBytes = std::array<std::uint8_t, kUnitIdWireSize>;

UnitIdBytes serialize(const UnitId& id) noexcept;

} // namespace hm::protocol
