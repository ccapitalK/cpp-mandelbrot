#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

using u32 = unsigned int;

static_assert(sizeof(u32) == 4);

template <typename F> void enforce(bool c, F errorMessage) {
    if (!c) {
        std::string message = errorMessage();
        std::cerr << "\n\nERROR: " << message << std::endl;
        exit(1);
    }
}

class MandelBrot {
  public:
    MandelBrot(size_t width_, size_t height_)
        : width(width_)
        , height(height_) {
        pixels = static_cast<u32*>(aligned_alloc(64, sizeof(u32) * width * height));
    }
    ~MandelBrot() {
        free(pixels);
        pixels = nullptr;
    }

    MandelBrot(const MandelBrot &) = delete;
    MandelBrot(MandelBrot &&) = delete;
    MandelBrot& operator=(const MandelBrot &) = delete;
    MandelBrot& operator=(MandelBrot &&) = delete;

    u32 &getPixelRef(size_t x, size_t y) { return pixels[y * width + x]; }

    size_t getWidth() const { return width; }

    size_t getHeight() const { return height; }

    std::pair<double, double> getPosition(size_t x, size_t y) {
        double px = x / (double)width;
        double py = y / (double)height;
        return {2 * px - 1.5, 2 * py - 1};
    }

    const u32 *getRGBA() const { return pixels; }

  private:
    const size_t width;
    const size_t height;
    u32* pixels;
};

unsigned char toU8(double v) { return v * 255.0; }

// clang-format off
// Assume little endian
u32 rgba(double r, double g, double b, double a) {
    return (toU8(r) << 0)
        |  (toU8(g) << 8)
        |  (toU8(b) << 16)
        |  (toU8(a) << 24);
}
// clang-format on

u32 colorPixel(float x, float y) {
    float cx = 0.0;
    float cy = 0.0;
    u32 iter = 0;
    while (iter < 256 && cx * cx + cy * cy < 2.0) {
        const float ncx = cx * cx - cy * cy;
        const float ncy = 2 * cx * cy;
        cx = ncx + x;
        cy = ncy + y;
        ++iter;
    }
    u32 light = 256 - iter;
    return rgba(light / 256.0, light / 256.0, 0, 1);
}

void writeImage(const std::string &filename, size_t width, size_t height, const u32 *data) {
    std::string_view view(filename);
    if (view.rfind(".png") != std::string_view::npos) {
        stbi_write_png(filename.c_str(), width, height, 4, data, width * 4);
    } else if (view.rfind(".bmp") != std::string_view::npos) {
        stbi_write_bmp(filename.c_str(), width, height, 4, data);
    } else {
        enforce(false, [&] { return std::string("Could not determine ext for filename \"") + filename + "\""; });
    }
}

void workerMain(MandelBrot &mandelbrot, u32 threadId, u32 numThreads) {
    // 64 byte cache line
    const size_t BLOCK_SIZE = 64 / sizeof(u32);
    const size_t w = mandelbrot.getWidth();
    const size_t h = mandelbrot.getHeight();
    size_t numPixels = w * h;
    for (size_t blockNum = threadId; blockNum * BLOCK_SIZE < numPixels; blockNum += numThreads) {
        for (size_t i = 0; i < BLOCK_SIZE && blockNum * BLOCK_SIZE + i < numPixels; ++i) {
            int pixelNum = blockNum * BLOCK_SIZE + i;
            size_t imageX = pixelNum % w;
            size_t imageY = pixelNum / w;
            auto pos = mandelbrot.getPosition(imageX, imageY);
            u32 pixel = colorPixel(std::get<0>(pos), std::get<1>(pos));
            mandelbrot.getPixelRef(imageX, imageY) = pixel;
        }
    }
}

int main(int argc, const char *argv[]) {
    enforce(argc == 3, [argv] { return (std::string{"Usage: "} + argv[0]) + " width height"; });

    size_t numThreads = 16;
    // TODO: Validate width and height
    u32 w = atoi(argv[1]);
    u32 h = atoi(argv[2]);
    MandelBrot mandelbrot(w, h);
    std::vector<std::thread> threads;

    for (size_t i = 0; i < numThreads; ++i) {
        threads.emplace_back([&mandelbrot, i, numThreads]() { workerMain(mandelbrot, i, numThreads); });
    }

    for (auto &thread : threads) {
        thread.join();
    }

    writeImage("output.bmp", mandelbrot.getWidth(), mandelbrot.getHeight(), mandelbrot.getRGBA());

    return 0;
}
