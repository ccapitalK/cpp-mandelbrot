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
        , height(height_)
        , pixels(width * height) {
        std::cout << pixels.size() << '\n';
    }

    u32 &getPixelRef(size_t x, size_t y) { return pixels[y * width + x]; }

    size_t getWidth() const { return width; }

    size_t getHeight() const { return height; }

    std::pair<double, double> getPosition(size_t x, size_t y) {
        double px = x / (double)width;
        double py = y / (double)height;
        return {2 * px - 1, 2 * py - 1};
    }

    const u32 *getRGBA() const { return pixels.data(); }

  private:
    const size_t width;
    const size_t height;
    std::vector<u32> pixels;
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

u32 colorPixel(double x, double y) { return rgba(1, 0, 0.5, 1); }

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
    const size_t w = mandelbrot.getWidth();
    const size_t h = mandelbrot.getHeight();
    size_t numPixels = w * h;
    // TODO: Stride pattern that doesn't result in false sharing :-)
    for (size_t i = threadId; i < numPixels; i += numThreads) {
        size_t imageX = i % w;
        size_t imageY = i / w;
        auto pos = mandelbrot.getPosition(imageX, imageY);
        u32 pixel = colorPixel(std::get<0>(pos), std::get<1>(pos));
        mandelbrot.getPixelRef(imageX, imageY) = pixel;
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
