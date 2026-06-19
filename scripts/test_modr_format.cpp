// Headless unit test for the .modr drawing file format used by DrawingApp.
// Compiles with only the C++ standard library (no SDL required).

#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

constexpr char kModrMagic[4] = {'M', 'O', 'D', 'R'};

void writeU32LE(std::string& out, uint32_t value) {
    out.push_back(static_cast<char>(value & 0xFF));
    out.push_back(static_cast<char>((value >> 8) & 0xFF));
    out.push_back(static_cast<char>((value >> 16) & 0xFF));
    out.push_back(static_cast<char>((value >> 24) & 0xFF));
}

uint32_t readU32LE(const std::string& data, size_t offset) {
    const auto* bytes = reinterpret_cast<const unsigned char*>(data.data() + offset);
    return static_cast<uint32_t>(bytes[0])
         | (static_cast<uint32_t>(bytes[1]) << 8)
         | (static_cast<uint32_t>(bytes[2]) << 16)
         | (static_cast<uint32_t>(bytes[3]) << 24);
}

std::string encodeCanvas(int width, int height, const std::vector<uint8_t>& rgb) {
    std::string blob;
    blob.append(kModrMagic, 4);
    writeU32LE(blob, static_cast<uint32_t>(width));
    writeU32LE(blob, static_cast<uint32_t>(height));
    for (uint8_t b : rgb) {
        blob.push_back(static_cast<char>(b));
    }
    return blob;
}

bool decodeCanvas(const std::string& blob, int& width, int& height, std::vector<uint8_t>& rgb) {
    if (blob.size() < 12 || std::memcmp(blob.data(), kModrMagic, 4) != 0) return false;
    width = static_cast<int>(readU32LE(blob, 4));
    height = static_cast<int>(readU32LE(blob, 8));
    if (width <= 0 || height <= 0) return false;
    const size_t expected = static_cast<size_t>(width) * static_cast<size_t>(height) * 3;
    if (blob.size() != 12 + expected) return false;
    rgb.assign(blob.begin() + 12, blob.end());
    return true;
}

int main() {
    int failures = 0;
    auto check = [&](bool ok, const char* msg) {
        if (!ok) {
            std::cerr << "FAIL: " << msg << '\n';
            ++failures;
        } else {
            std::cout << "ok: " << msg << '\n';
        }
    };

    // 2x2 canvas with distinct pixels
    const std::vector<uint8_t> original = {
        10, 20, 30,   40, 50, 60,
        70, 80, 90,   100, 110, 120
    };
    const std::string encoded = encodeCanvas(2, 2, original);

    check(encoded.size() == 12 + 12, "encoded size");
    check(std::memcmp(encoded.data(), "MODR", 4) == 0, "magic bytes");
    check(readU32LE(encoded, 4) == 2, "width field");
    check(readU32LE(encoded, 8) == 2, "height field");

    int w = 0;
    int h = 0;
    std::vector<uint8_t> decoded;
    check(decodeCanvas(encoded, w, h, decoded), "decode succeeds");
    check(w == 2 && h == 2, "decoded dimensions");
    check(decoded == original, "roundtrip pixel payload");

    check(!decodeCanvas("BAD!", w, h, decoded), "reject bad magic");
    check(!decodeCanvas(encoded.substr(0, 10), w, h, decoded), "reject truncated file");

    if (failures == 0) {
        std::cout << "ALL MODR TESTS PASSED\n";
        return 0;
    }
    std::cerr << failures << " test(s) failed\n";
    return 1;
}