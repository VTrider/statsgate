#include "bzreplay.h"

#include <zpp_bits.h>

#include <fstream>

namespace bzreplay
{
    replay::replay() {}

    void replay::write_file(const std::filesystem::path& path)
    {
        auto [data, out] = zpp::bits::data_out();
        auto error = out(header);
        if (zpp::bits::failure(error))
        {

        }
        std::ofstream file(path);
        file.write(reinterpret_cast<const char*>(data.data()), sizeof(decltype(data)::value_type) * data.size());
        file.close();
    }
}
