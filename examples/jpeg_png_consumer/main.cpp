#define NOMINMAX
#include <CcpCore.h>
#include <HostBitmap.h>
#include <Tr2ImageHandler.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

const char* g_moduleName = "imageio_jpeg_png_consumer";

namespace {

class MemoryStream : public ICcpStream {
public:
    explicit MemoryStream(std::vector<unsigned char> initial = {})
        : data(std::move(initial)) {}

    ptrdiff_t Read(void* dest, ptrdiff_t count) override {
        if (count < 0 || pos > data.size()) {
            return -1;
        }
        const size_t available = data.size() - pos;
        const size_t n = std::min(static_cast<size_t>(count), available);
        if (n != 0) {
            std::memcpy(dest, data.data() + pos, n);
        }
        pos += n;
        return static_cast<ptrdiff_t>(n);
    }

    ptrdiff_t Write(const void* source, size_t count) override {
        if (pos > data.size()) {
            return -1;
        }
        if (pos + count > data.size()) {
            data.resize(pos + count);
        }
        if (count != 0) {
            std::memcpy(data.data() + pos, source, count);
        }
        pos += count;
        return static_cast<ptrdiff_t>(count);
    }

    ptrdiff_t Seek(ptrdiff_t distance, SeekOrigin method) override {
        ptrdiff_t next = 0;
        const ptrdiff_t size = static_cast<ptrdiff_t>(data.size());
        if (method == SO_BEGIN) {
            next = distance;
        } else if (method == SO_CURRENT) {
            next = static_cast<ptrdiff_t>(pos) + distance;
        } else if (method == SO_END) {
            next = size - distance;
        } else {
            return -1;
        }
        if (next < 0 || next > size) {
            return -1;
        }
        pos = static_cast<size_t>(next);
        return static_cast<ptrdiff_t>(pos);
    }

    ptrdiff_t GetPosition() override { return static_cast<ptrdiff_t>(pos); }
    ptrdiff_t GetSize() override { return static_cast<ptrdiff_t>(data.size()); }

    void Rewind() { pos = 0; }

private:
    std::vector<unsigned char> data;
    size_t pos = 0;
};

int fail(const char* message) {
    std::fprintf(stderr, "IMAGEIO_JPEG_PNG_CONSUMER_FAILED %s\n", message);
    return 1;
}

void put_bgra(ImageIO::HostBitmap& bitmap, unsigned x, unsigned y, uint8_t b, uint8_t g, uint8_t r, uint8_t a) {
    auto* pixel = reinterpret_cast<unsigned char*>(bitmap.GetRawData(x, y));
    pixel[0] = b;
    pixel[1] = g;
    pixel[2] = r;
    pixel[3] = a;
}

bool same_bgra(const ImageIO::HostBitmap& bitmap, unsigned x, unsigned y, uint8_t b, uint8_t g, uint8_t r, uint8_t a) {
    const auto* pixel = reinterpret_cast<const unsigned char*>(bitmap.GetRawData(x, y));
    return pixel != nullptr && pixel[0] == b && pixel[1] == g && pixel[2] == r && pixel[3] == a;
}

}  // namespace

int main() {
    constexpr unsigned kPngWidth = 4;
    constexpr unsigned kPngHeight = 3;
    ImageIO::HostBitmap pngSource;
    if (!pngSource.Create(kPngWidth, kPngHeight, 1, ImageIO::PIXEL_FORMAT_B8G8R8A8_UNORM)) {
        return fail("png bitmap create");
    }
    for (unsigned y = 0; y < kPngHeight; ++y) {
        for (unsigned x = 0; x < kPngWidth; ++x) {
            put_bgra(pngSource, x, y,
                     static_cast<uint8_t>(10 + x),
                     static_cast<uint8_t>(20 + y),
                     30,
                     static_cast<uint8_t>(40 + x + y));
        }
    }

    MemoryStream pngStream;
    const ImageIO::Result pngSaved = ImageIO::SaveImage(L"fixture.png", pngSource, pngStream);
    if (!pngSaved || pngStream.GetSize() < 8) {
        return fail("png save");
    }
    pngStream.Rewind();
    ImageIO::HostBitmap pngLoaded;
    const ImageIO::Result pngRead = ImageIO::ReadImage(
        pngStream, ImageIO::LoadParameters(L"fixture.png"), pngLoaded);
    if (!pngRead) {
        return fail("png read");
    }
    if (pngLoaded.GetWidth() != kPngWidth || pngLoaded.GetHeight() != kPngHeight ||
        pngLoaded.GetFormat() != ImageIO::PIXEL_FORMAT_B8G8R8A8_UNORM) {
        return fail("png dimensions");
    }
    for (unsigned y = 0; y < kPngHeight; ++y) {
        for (unsigned x = 0; x < kPngWidth; ++x) {
            if (!same_bgra(pngLoaded, x, y,
                           static_cast<uint8_t>(10 + x),
                           static_cast<uint8_t>(20 + y),
                           30,
                           static_cast<uint8_t>(40 + x + y))) {
                return fail("png pixels");
            }
        }
    }

    constexpr unsigned kJpegSize = 8;
    constexpr uint8_t kGray = 128;
    ImageIO::HostBitmap jpegSource;
    if (!jpegSource.Create(kJpegSize, kJpegSize, 1, ImageIO::PIXEL_FORMAT_R8_UNORM)) {
        return fail("jpeg bitmap create");
    }
    std::memset(jpegSource.GetRawData(), kGray, jpegSource.GetRawDataSize());
    MemoryStream jpegStream;
    const ImageIO::Result jpegSaved = ImageIO::SaveImage(L"fixture.jpg", jpegSource, jpegStream);
    if (!jpegSaved || jpegStream.GetSize() < 4) {
        return fail("jpeg save");
    }
    jpegStream.Rewind();
    ImageIO::HostBitmap jpegLoaded;
    const ImageIO::Result jpegRead = ImageIO::ReadImage(
        jpegStream, ImageIO::LoadParameters(L"fixture.jpg"), jpegLoaded);
    if (!jpegRead) {
        return fail("jpeg read");
    }
    if (jpegLoaded.GetWidth() != kJpegSize || jpegLoaded.GetHeight() != kJpegSize ||
        jpegLoaded.GetFormat() != ImageIO::PIXEL_FORMAT_B8G8R8A8_UNORM) {
        return fail("jpeg dimensions");
    }
    for (unsigned y = 0; y < kJpegSize; ++y) {
        for (unsigned x = 0; x < kJpegSize; ++x) {
            if (!same_bgra(jpegLoaded, x, y, kGray, kGray, kGray, 255)) {
                const auto* pixel = reinterpret_cast<const unsigned char*>(jpegLoaded.GetRawData(x, y));
                std::fprintf(stderr, "IMAGEIO_JPEG_PNG_CONSUMER_FAILED jpeg pixel %u,%u = %u %u %u %u\n",
                             x, y, pixel[0], pixel[1], pixel[2], pixel[3]);
                return 1;
            }
        }
    }

    const unsigned char truncated[] = {0xff, 0xd8, 0x00};
    MemoryStream badStream(std::vector<unsigned char>(truncated, truncated + sizeof(truncated)));
    ImageIO::HostBitmap badBitmap;
    const ImageIO::Result badRead = ImageIO::ReadImage(
        badStream, ImageIO::LoadParameters(L"truncated.jpg"), badBitmap);
    if (badRead) {
        return fail("truncated jpeg was accepted");
    }

    ImageIO::HostBitmap unsupported;
    if (!unsupported.Create(2, 2, 1, ImageIO::PIXEL_FORMAT_R8G8_UNORM)) {
        return fail("unsupported bitmap create");
    }
    MemoryStream rejected;
    const ImageIO::Result rejectedSave = ImageIO::SaveImage(L"rejected.png", unsupported, rejected);
    if (rejectedSave || rejected.GetSize() != 0) {
        return fail("unsupported png save was accepted");
    }

    std::printf(
        "IMAGEIO_JPEG_PNG_CONSUMER png=%ux%u bytes=%lld jpeg=%ux%u bytes=%lld gray=%u invalid=%d unsupported=%d\n",
        pngLoaded.GetWidth(), pngLoaded.GetHeight(), static_cast<long long>(pngStream.GetSize()),
        jpegLoaded.GetWidth(), jpegLoaded.GetHeight(), static_cast<long long>(jpegStream.GetSize()),
        kGray, static_cast<int>(badRead.code), static_cast<int>(rejectedSave.code));
    return 0;
}
