#include "../DrawComponents.h"

namespace DRAW {
    inline bool ReadFileBytes(const std::string& filePath, std::vector<unsigned char>& outBytes)
    {
        std::ifstream file(filePath, std::ios::binary);
        if (!file) return false;

        file.seekg(0, std::ios::end);
        size_t size = static_cast<size_t>(file.tellg());
        file.seekg(0, std::ios::beg);

        outBytes.resize(size);
        file.read(reinterpret_cast<char*>(outBytes.data()), size);
        return true;
    }
}

