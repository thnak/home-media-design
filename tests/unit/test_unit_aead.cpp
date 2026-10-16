// Kiem tra cac tinh chat bat buoc cua don vi ma hoa (thiet ke muc 3.3 va muc 15).
#include "hm/crypto/unit_aead.hpp"

#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

int g_failures = 0;

void check(bool ok, const char* what) {
    std::printf("[%s] %s\n", ok ? " OK " : "FAIL", what);
    if (!ok) {
        ++g_failures;
    }
}

hm::protocol::UnitId make_id(std::size_t len) {
    hm::protocol::UnitId id;
    id.asset_id.fill(0xAB);
    id.generation = 1;
    id.variant_id = 0;
    id.unit_index = 7;
    id.plaintext_len = static_cast<std::uint32_t>(len);
    return id;
}

bool opens(const hm::crypto::SecretKey& key, const hm::protocol::UnitId& id,
           const std::vector<std::uint8_t>& unit, std::vector<std::uint8_t>& out) {
    out.assign(id.plaintext_len, 0);
    return hm::crypto::open_unit_into(key, id, unit, out);
}

} // namespace

int main() {
    if (!hm::crypto::initialize()) {
        std::fprintf(stderr, "libsodium init failed\n");
        return EXIT_FAILURE;
    }

    std::vector<std::uint8_t> plain(1 << 20);
    for (std::size_t i = 0; i < plain.size(); ++i) {
        plain[i] = static_cast<std::uint8_t>(i * 31u);
    }

    hm::crypto::SecretKey dek;
    const auto key = dek.derive_variant_key(0);
    const auto id = make_id(plain.size());
    const auto unit = hm::crypto::seal_unit(key, id, plain);
    std::vector<std::uint8_t> out;

    check(unit.size() == plain.size() + hm::crypto::kUnitOverhead, "kich thuoc = plaintext + 40 byte");
    check(opens(key, id, unit, out) && out == plain, "ma hoa roi mo lai dung du lieu");

    auto tampered = unit;
    tampered[tampered.size() / 2] ^= 0x01;
    check(!opens(key, id, tampered, out), "sua 1 bit ciphertext bi tu choi");

    auto bad_index = id;
    bad_index.unit_index += 1;
    check(!opens(key, bad_index, unit, out), "tra vi tri (unit_index) bi tu choi");

    auto bad_variant = id;
    bad_variant.variant_id = 3;
    check(!opens(key, bad_variant, unit, out), "tra variant bi tu choi");

    auto bad_gen = id;
    bad_gen.generation = 2;
    check(!opens(key, bad_gen, unit, out), "tra generation bi tu choi");

    auto bad_asset = id;
    bad_asset.asset_id[0] ^= 0xFF;
    check(!opens(key, bad_asset, unit, out), "tra asset bi tu choi");

    const auto other_variant_key = dek.derive_variant_key(1);
    check(!opens(other_variant_key, id, unit, out), "khoa cua variant khac bi tu choi");

    std::vector<std::uint8_t> truncated(unit.begin(), unit.end() - 1);
    check(!opens(key, id, truncated, out), "don vi bi cat cut bi tu choi");

    const auto unit2 = hm::crypto::seal_unit(key, id, plain);
    check(unit2 != unit, "hai lan ma hoa cung du lieu cho ket qua khac (nonce moi)");

    std::printf("%s (%d loi)\n", g_failures == 0 ? "PASS" : "FAIL", g_failures);
    return g_failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
