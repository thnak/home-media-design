// media_worker: tien trinh giu khoa. Chua co chuc nang; giu cho ranh gioi target.
#include "hm/crypto/unit_aead.hpp"

#include <cstdio>

int main() {
    if (!hm::crypto::initialize()) {
        std::fprintf(stderr, "libsodium init failed\n");
        return 1;
    }
    std::printf("media_worker: chua trien khai\n");
    return 0;
}
