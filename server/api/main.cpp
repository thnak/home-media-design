// media_api: dich vu tiep xuc mang. Chua co chuc nang; giu cho ranh gioi target.
#include "hm/protocol/unit_id.hpp"

#include <cstdio>

int main() {
    std::printf("media_api (format v%u): chua trien khai\n",
                static_cast<unsigned>(hm::protocol::kFormatVersion));
    return 0;
}
