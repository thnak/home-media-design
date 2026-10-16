// Benchmark B1: thong luong ma hoa/giai ma theo kich thuoc don vi (thiet ke muc 3.2, 16).
// Ket qua quyet dinh kich thuoc don vi luu tru va so don vi gop trong mot request.
//
// Cach chay:  bench_crypto_chunk [tong_MiB=2048] > ../bench/results/crypto_<may>.csv
#include "hm/crypto/unit_aead.hpp"

#include <sodium.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

double seconds_since(Clock::time_point t0) {
    return std::chrono::duration<double>(Clock::now() - t0).count();
}

} // namespace

int main(int argc, char** argv) {
    if (!hm::crypto::initialize()) {
        std::fprintf(stderr, "libsodium init failed\n");
        return EXIT_FAILURE;
    }
    const std::size_t total_mib = argc > 1 ? std::stoul(argv[1]) : 2048;
    const std::size_t total_bytes = total_mib << 20;

    std::fprintf(stderr, "libsodium %s, AVX2=%d, AVX512F=%d, tong %zu MiB moi kich thuoc\n",
                 sodium_version_string(), sodium_runtime_has_avx2(), sodium_runtime_has_avx512f(),
                 total_mib);

    const std::size_t sizes_kib[] = {64, 256, 1024, 2048, 4096, 8192, 16384};

    hm::crypto::SecretKey key;
    std::printf("unit_kib,units,seal_gib_s,open_gib_s,overhead_pct\n");

    for (const std::size_t kib : sizes_kib) {
        const std::size_t unit_bytes = kib << 10;
        const std::size_t units = std::max<std::size_t>(1, total_bytes / unit_bytes);

        std::vector<std::uint8_t> plain(unit_bytes);
        randombytes_buf(plain.data(), plain.size());
        std::vector<std::uint8_t> sealed(unit_bytes + hm::crypto::kUnitOverhead);
        std::vector<std::uint8_t> opened(unit_bytes);

        hm::protocol::UnitId id;
        id.plaintext_len = static_cast<std::uint32_t>(unit_bytes);

        auto t0 = Clock::now();
        for (std::size_t i = 0; i < units; ++i) {
            id.unit_index = i;
            hm::crypto::seal_unit_into(key, id, plain, sealed);
        }
        const double seal_s = seconds_since(t0);

        // Mo lai don vi cuoi; cac lan mo dung cung index de do thong luong thuan.
        bool all_ok = true;
        t0 = Clock::now();
        for (std::size_t i = 0; i < units; ++i) {
            all_ok &= hm::crypto::open_unit_into(key, id, sealed, opened);
        }
        const double open_s = seconds_since(t0);
        if (!all_ok || opened != plain) {
            std::fprintf(stderr, "LOI: giai ma that bai o %zu KiB\n", kib);
            return EXIT_FAILURE;
        }

        const double gib = static_cast<double>(units * unit_bytes) / (1024.0 * 1024.0 * 1024.0);
        const double overhead =
            100.0 * static_cast<double>(hm::crypto::kUnitOverhead) / static_cast<double>(unit_bytes);
        std::printf("%zu,%zu,%.2f,%.2f,%.4f\n", kib, units, gib / seal_s, gib / open_s, overhead);
        std::fflush(stdout);
    }
    return EXIT_SUCCESS;
}
