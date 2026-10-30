#include "bzreplay.h"

#include <fast_io.h>
#include <zpp_bits.h>

#include <fstream>
#include <print>

int main ()
{
    bzreplay::header h{};
    // bzreplay::object_state obj;
    bzreplay::replay::object_state_soa obj{};
    obj.player.emplace_back(15);
    h.author_nickname = "VTrider";

    auto [data, in, out] = zpp::bits::data_in_out();
    auto r = out(obj);

    if (zpp::bits::failure(r))
        std::println("error");

    std::ofstream f("test.bin", std::ios::binary);
    f.write((const char*)data.data(), data.size());
    return 0;
}
