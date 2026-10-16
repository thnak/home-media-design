#include "hm/crypto/unit_aead.hpp"

#include <sodium.h>

#include <cstring>
#include <new>
#include <stdexcept>
#include <utility>

namespace hm::crypto {

static_assert(kKeySize == crypto_aead_xchacha20poly1305_ietf_KEYBYTES);
static_assert(kNonceSize == crypto_aead_xchacha20poly1305_ietf_NPUBBYTES);
static_assert(kTagSize == crypto_aead_xchacha20poly1305_ietf_ABYTES);

namespace {

std::uint8_t* alloc_key() {
    auto* p = static_cast<std::uint8_t*>(sodium_malloc(kKeySize));
    if (p == nullptr) {
        throw std::bad_alloc();
    }
    return p;
}

// Ngu canh KDF co dinh 8 ky tu, theo yeu cau cua crypto_kdf.
constexpr char kVariantKdfContext[crypto_kdf_CONTEXTBYTES + 1] = "hmvarkey";

} // namespace

bool initialize() noexcept { return sodium_init() >= 0; }

SecretKey::SecretKey() : data_(alloc_key()) { crypto_aead_xchacha20poly1305_ietf_keygen(data_); }

SecretKey::SecretKey(Uninitialized) : data_(alloc_key()) {}

SecretKey::~SecretKey() {
    if (data_ != nullptr) {
        sodium_free(data_); // sodium_free xoa vung nho truoc khi giai phong
    }
}

SecretKey::SecretKey(SecretKey&& other) noexcept : data_(std::exchange(other.data_, nullptr)) {}

SecretKey& SecretKey::operator=(SecretKey&& other) noexcept {
    if (this != &other) {
        if (data_ != nullptr) {
            sodium_free(data_);
        }
        data_ = std::exchange(other.data_, nullptr);
    }
    return *this;
}

SecretKey SecretKey::derive_variant_key(std::uint32_t variant_id) const {
    static_assert(crypto_kdf_KEYBYTES == kKeySize);
    SecretKey sub(Uninitialized{});
    if (crypto_kdf_derive_from_key(sub.data_, kKeySize, variant_id, kVariantKdfContext, data_) != 0) {
        throw std::runtime_error("crypto_kdf_derive_from_key failed");
    }
    return sub;
}

std::span<const std::uint8_t, kKeySize> SecretKey::bytes() const noexcept {
    return std::span<const std::uint8_t, kKeySize>(data_, kKeySize);
}

std::size_t seal_unit_into(const SecretKey& key, const protocol::UnitId& id,
                           std::span<const std::uint8_t> plaintext, std::span<std::uint8_t> out) {
    if (id.plaintext_len != plaintext.size()) {
        throw std::invalid_argument("UnitId.plaintext_len does not match plaintext size");
    }
    if (out.size() < plaintext.size() + kUnitOverhead) {
        throw std::invalid_argument("output buffer too small");
    }
    const auto aad = protocol::serialize(id);
    std::uint8_t* nonce = out.data();
    randombytes_buf(nonce, kNonceSize); // nonce ngau nhien 192 bit: khong lo trung lap
    unsigned long long clen = 0;
    crypto_aead_xchacha20poly1305_ietf_encrypt(out.data() + kNonceSize, &clen, plaintext.data(),
                                               plaintext.size(), aad.data(), aad.size(), nullptr,
                                               nonce, key.bytes().data());
    return kNonceSize + static_cast<std::size_t>(clen);
}

std::vector<std::uint8_t> seal_unit(const SecretKey& key, const protocol::UnitId& id,
                                    std::span<const std::uint8_t> plaintext) {
    std::vector<std::uint8_t> out(plaintext.size() + kUnitOverhead);
    out.resize(seal_unit_into(key, id, plaintext, out));
    return out;
}

bool open_unit_into(const SecretKey& key, const protocol::UnitId& id,
                    std::span<const std::uint8_t> unit, std::span<std::uint8_t> out) noexcept {
    if (unit.size() < kUnitOverhead) {
        return false;
    }
    const std::size_t plain_len = unit.size() - kUnitOverhead;
    if (plain_len != id.plaintext_len || out.size() < plain_len) {
        return false;
    }
    const auto aad = protocol::serialize(id);
    unsigned long long mlen = 0;
    const int rc = crypto_aead_xchacha20poly1305_ietf_decrypt(
        out.data(), &mlen, nullptr, unit.data() + kNonceSize, unit.size() - kNonceSize, aad.data(),
        aad.size(), unit.data(), key.bytes().data());
    return rc == 0 && mlen == plain_len;
}

} // namespace hm::crypto
