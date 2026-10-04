// Headless regression test for the production .modr codec.

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <ostream>
#include <sstream>
#include <streambuf>
#include <string>
#include <string_view>
#include <vector>

#include "../src/app/DrawingRaster.hpp"

namespace {

constexpr char kModrMagic[4] = {'M', 'O', 'D', 'R'};

class CountingStreamBuffer final : public std::streambuf {
public:
    std::size_t bytesWritten = 0;
    std::size_t largestWrite = 0;

protected:
    std::streamsize xsputn(const char*, std::streamsize count) override {
        const auto size = static_cast<std::size_t>(count);
        bytesWritten += size;
        largestWrite = std::max(largestWrite, size);
        return count;
    }
};

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

std::string modrHeader(uint32_t width, uint32_t height) {
    std::string blob(kModrMagic, sizeof(kModrMagic));
    writeU32LE(blob, width);
    writeU32LE(blob, height);
    return blob;
}

} // namespace

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

    const std::vector<uint8_t> original = {
        10, 20, 30, 0,   40, 50, 60, 128,
        70, 80, 90, 1,   100, 110, 120, 255
    };
    const std::string encoded = monolith::drawing::encodeModr(2, 2, original);
    check(encoded.size() == 12 + 12, "production encoder writes RGB payload size");
    check(encoded.size() >= 12
              && std::string(encoded.data(), 4) == "MODR"
              && readU32LE(encoded, 4) == 2
              && readU32LE(encoded, 8) == 2,
          "production encoder writes magic and little-endian dimensions");
    std::ostringstream streamed;
    check(monolith::drawing::writeModr(streamed, 2, 2, original)
              && streamed.str() == encoded,
          "streaming encoder preserves exact production format bytes");
    check(monolith::drawing::matchesModrChunk(
              2, 2, original, 0, std::string_view(encoded).substr(0, 7))
              && monolith::drawing::matchesModrChunk(
                  2, 2, original, 7, std::string_view(encoded).substr(7)),
          "saved-canvas comparison handles chunks split across the header and RGB payload");
    std::string changedChunk = encoded.substr(4, 4);
    changedChunk[2] ^= 1;
    check(!monolith::drawing::matchesModrChunk(2, 2, original, 4, changedChunk)
              && !monolith::drawing::matchesModrChunk(
                  2, 2, original, encoded.size(), "x"),
          "saved-canvas comparison rejects changed bytes and ranges beyond the encoded file");

    monolith::drawing::ModrStreamDecoder chunkDecoder(encoded.size());
    constexpr std::size_t chunkSizes[] = {1, 2, 5, 3, 1, 4};
    std::size_t chunkOffset = 0;
    std::size_t chunkIndex = 0;
    bool chunksAccepted = true;
    while (chunkOffset < encoded.size()) {
        const std::size_t chunkSize = std::min(
            chunkSizes[chunkIndex % std::size(chunkSizes)],
            encoded.size() - chunkOffset);
        chunksAccepted = chunkDecoder.consume(std::string_view(
            encoded.data() + chunkOffset, chunkSize));
        if (!chunksAccepted) break;
        chunkOffset += chunkSize;
        ++chunkIndex;
    }
    int streamedWidth = 0;
    int streamedHeight = 0;
    std::vector<uint8_t> streamedPixels;
    check(chunksAccepted
              && chunkDecoder.finish(streamedWidth, streamedHeight, streamedPixels)
              && streamedWidth == 2 && streamedHeight == 2
              && streamedPixels == std::vector<uint8_t>({
                  10, 20, 30, 255,   40, 50, 60, 255,
                  70, 80, 90, 255,   100, 110, 120, 255}),
          "streaming decoder handles header and RGB triplets split across chunks");

    monolith::drawing::ModrStreamDecoder truncatedDecoder(encoded.size() - 1);
    int preservedWidth = 71;
    int preservedHeight = 82;
    std::vector<uint8_t> preservedPixels = {9, 8, 7};
    const bool truncatedAccepted = truncatedDecoder.consume(
        std::string_view(encoded.data(), encoded.size() - 1));
    check(!truncatedAccepted
              && !truncatedDecoder.finish(
                  preservedWidth, preservedHeight, preservedPixels)
              && preservedWidth == 71 && preservedHeight == 82
              && preservedPixels == std::vector<uint8_t>({9, 8, 7}),
          "streaming decoder rejects mismatched total size without replacing outputs");

    monolith::drawing::ModrStreamDecoder incompleteDecoder(encoded.size());
    const bool partialAccepted = incompleteDecoder.consume(std::string_view(
        encoded.data(), encoded.size() - 1));
    check(partialAccepted
              && !incompleteDecoder.finish(
                  preservedWidth, preservedHeight, preservedPixels)
              && preservedWidth == 71 && preservedHeight == 82
              && preservedPixels == std::vector<uint8_t>({9, 8, 7}),
          "streaming decoder rejects incomplete final payload without replacing outputs");

    int width = 0;
    int height = 0;
    std::vector<uint8_t> decoded;
    check(monolith::drawing::decodeModr(encoded, width, height, decoded),
          "production decoder accepts an encoded canvas");
    check(width == 2 && height == 2, "production decoder restores dimensions");
    check(decoded == std::vector<uint8_t>({
              10, 20, 30, 255,   40, 50, 60, 255,
              70, 80, 90, 255,   100, 110, 120, 255}),
          "production codec roundtrip preserves RGB and restores opaque alpha");

    const auto rejectsWithoutMutation = [&](const std::string& blob, const char* msg) {
        width = 77;
        height = 88;
        decoded = {9, 8, 7};
        const bool rejected = !monolith::drawing::decodeModr(blob, width, height, decoded);
        check(rejected && width == 77 && height == 88
                  && decoded == std::vector<uint8_t>({9, 8, 7}),
              msg);
    };

    std::string badMagic = encoded;
    badMagic[0] = 'X';
    rejectsWithoutMutation(badMagic, "reject bad magic without replacing output");
    rejectsWithoutMutation(encoded.substr(0, 10),
                           "reject truncated header without replacing output");
    rejectsWithoutMutation(encoded.substr(0, encoded.size() - 1),
                           "reject truncated payload without replacing output");
    rejectsWithoutMutation(encoded + "x",
                           "reject trailing payload bytes without replacing output");
    rejectsWithoutMutation(modrHeader(0, 1),
                           "reject zero dimensions without replacing output");
    rejectsWithoutMutation(modrHeader(
                              static_cast<uint32_t>(monolith::drawing::kMaxModrDimension + 1), 1),
                           "reject dimensions beyond the supported maximum");
    rejectsWithoutMutation(modrHeader(0x80000000u, 1),
                           "reject high-bit dimensions before signed conversion");
    rejectsWithoutMutation(modrHeader(1, 0xFFFFFFFFu),
                           "reject maximum unsigned height without replacing output");

    const std::vector<uint8_t> onePixel = {1, 2, 3, 255};
    check(monolith::drawing::encodeModr(0, 1, onePixel).empty(),
          "encoder rejects zero dimensions");
    check(monolith::drawing::encodeModr(
              monolith::drawing::kMaxModrDimension + 1, 1,
              std::vector<uint8_t>(static_cast<size_t>(monolith::drawing::kMaxModrDimension + 1) * 4, 255))
              .empty(),
          "encoder rejects dimensions the decoder cannot open");
    check(monolith::drawing::encodeModr(1, 1, {1, 2, 3}).empty(),
          "encoder rejects a short RGBA buffer");
    check(monolith::drawing::encodeModr(1, 1, {1, 2, 3, 255, 4}).empty(),
          "encoder rejects an oversized RGBA buffer");
    std::ostringstream invalidStream;
    check(!monolith::drawing::writeModr(invalidStream, 1, 1, {1, 2, 3})
              && invalidStream.str().empty(),
          "streaming encoder rejects invalid buffers before writing");

    std::vector<uint8_t> maximumRow(
        static_cast<size_t>(monolith::drawing::kMaxModrDimension) * 4, 127);
    const std::string maximumEncoded = monolith::drawing::encodeModr(
        monolith::drawing::kMaxModrDimension, 1, maximumRow);
    width = 0;
    height = 0;
    decoded.clear();
    check(!maximumEncoded.empty()
              && monolith::drawing::decodeModr(maximumEncoded, width, height, decoded)
              && width == monolith::drawing::kMaxModrDimension && height == 1
              && decoded.size() == maximumRow.size(),
          "encoder and decoder agree on the supported dimension boundary");

    const std::size_t maximumPixels =
        static_cast<std::size_t>(monolith::drawing::kMaxModrDimension)
        * static_cast<std::size_t>(monolith::drawing::kMaxModrDimension);
    const std::vector<uint8_t> maximumCanvas(maximumPixels * 4, 127);
    CountingStreamBuffer boundedOutput;
    std::ostream maximumStream(&boundedOutput);
    check(monolith::drawing::writeModr(
              maximumStream,
              monolith::drawing::kMaxModrDimension,
              monolith::drawing::kMaxModrDimension,
              maximumCanvas)
              && boundedOutput.bytesWritten == monolith::drawing::kMaxModrEncodedBytes
              && boundedOutput.largestWrite <= 16 * 1024,
          "maximum-size canvas streams with bounded writes and no encoded-size buffer");

    if (failures == 0) {
        std::cout << "ALL MODR TESTS PASSED\n";
        return 0;
    }
    std::cerr << failures << " test(s) failed\n";
    return 1;
}
