#include "hm/protocol/unit_id.hpp"

#include <algorithm>

namespace hm::protocol {
namespace {

template <typename T>
std::size_t put_le(UnitIdBytes& out, std::size_t pos, T value) noexcept {
    for (std::size_t i = 0; i < sizeof(T); ++i) {
        out[pos + i] = static_cast<std::uint8_t>(value >> (8 * i));
    }
    return pos + sizeof(T);
}

} // namespace

UnitIdBytes serialize(const UnitId& id) noexcept {
    UnitIdBytes out{};
    std::size_t pos = 0;
    pos = put_le(out, pos, id.format_version);
    out[pos++] = static_cast<std::uint8_t>(id.kind);
    std::copy(id.asset_id.begin(), id.asset_id.end(), out.begin() + static_cast<std::ptrdiff_t>(pos));
    pos += id.asset_id.size();
    pos = put_le(out, pos, id.generation);
    pos = put_le(out, pos, id.variant_id);
    pos = put_le(out, pos, id.unit_index);
    pos = put_le(out, pos, id.plaintext_len);
    return out;
}

} // namespace hm::protocol
