#include "rounds/OcrImagePrep.h"

#include <algorithm>
#include <cstddef>

namespace tft {
namespace {

int ClampInt(int value, int low, int high) {
    return std::clamp(value, low, high);
}

int Luma(std::uint8_t b, std::uint8_t g, std::uint8_t r) {
    return (77 * static_cast<int>(b) + 150 * static_cast<int>(g)
            + 29 * static_cast<int>(r)) / 256;
}

} // namespace

bool PrepareOcrBitmap(const std::uint8_t* src,
                      int src_width,
                      int src_height,
                      int src_stride,
                      const ImageRect* rect,
                      int scale,
                      int contrast_percent,
                      std::vector<std::uint8_t>& out,
                      int& out_width,
                      int& out_height) {
    if (!src || src_width <= 0 || src_height <= 0 || src_stride < src_width * 4) return false;
    if (scale < 1 || scale > 4 || contrast_percent < 50 || contrast_percent > 300) return false;

    ImageRect rc = rect ? *rect : ImageRect{0, 0, src_width, src_height};
    rc.x = std::max(0, rc.x);
    rc.y = std::max(0, rc.y);
    rc.w = std::min(rc.w, src_width - rc.x);
    rc.h = std::min(rc.h, src_height - rc.y);
    if (rc.w <= 0 || rc.h <= 0) return false;

    out_width = rc.w * scale;
    out_height = rc.h * scale;
    out.assign(static_cast<std::size_t>(out_width) * out_height * 4, 0);

    for (int oy = 0; oy < out_height; ++oy) {
        const int sy = rc.y + oy / scale;
        const auto* src_row = src + static_cast<std::size_t>(sy) * src_stride;
        auto* dst_row = out.data() + static_cast<std::size_t>(oy) * out_width * 4;
        for (int ox = 0; ox < out_width; ++ox) {
            const int sx = rc.x + ox / scale;
            const auto* pixel = src_row + static_cast<std::size_t>(sx) * 4;
            const int gray = Luma(pixel[0], pixel[1], pixel[2]);
            const int value = ClampInt(128 + (gray - 128) * contrast_percent / 100, 0, 255);
            auto* dst = dst_row + static_cast<std::size_t>(ox) * 4;
            dst[0] = static_cast<std::uint8_t>(value);
            dst[1] = static_cast<std::uint8_t>(value);
            dst[2] = static_cast<std::uint8_t>(value);
            dst[3] = 255;
        }
    }
    return true;
}

std::vector<std::uint8_t> MakeBgraSignature(const std::uint8_t* src,
                                            int src_width,
                                            int src_height,
                                            int src_stride,
                                            int out_width,
                                            int out_height) {
    if (!src || src_width <= 0 || src_height <= 0 || src_stride < src_width * 4
        || out_width <= 0 || out_height <= 0) {
        return {};
    }

    std::vector<std::uint8_t> out(static_cast<std::size_t>(out_width) * out_height, 0);
    for (int oy = 0; oy < out_height; ++oy) {
        const int y0 = oy * src_height / out_height;
        const int y1 = std::max(y0 + 1, (oy + 1) * src_height / out_height);
        for (int ox = 0; ox < out_width; ++ox) {
            const int x0 = ox * src_width / out_width;
            const int x1 = std::max(x0 + 1, (ox + 1) * src_width / out_width);
            unsigned long long sum = 0;
            int count = 0;
            for (int y = y0; y < y1; ++y) {
                const auto* row = src + static_cast<std::size_t>(y) * src_stride;
                for (int x = x0; x < x1; ++x) {
                    const auto* pixel = row + static_cast<std::size_t>(x) * 4;
                    sum += static_cast<unsigned>(Luma(pixel[0], pixel[1], pixel[2]));
                    ++count;
                }
            }
            out[static_cast<std::size_t>(oy) * out_width + ox] =
                static_cast<std::uint8_t>(count ? sum / static_cast<unsigned>(count) : 0);
        }
    }
    return out;
}

} // namespace tft
