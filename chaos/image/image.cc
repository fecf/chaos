#include "image.h"

#include <cassert>

#include "base/minlog.h"

// readers
#include "pnm_rw.h"
#include "stb_rw.h"
#include "wic_rw.h"
#include "winrt_rw.h"

namespace chaos {

inline size_t getPitch(PixelFormat format, int width) noexcept {
  if (format == PixelFormat::RGBA8) {
    return width * 4;
  } else if (format == PixelFormat::RGBA16) {
    return width * 2 * 4;
  } else if (format == PixelFormat::RGBA32F) {
    return width * 4 * 4;
  }
  assert(false && "not implemented.");
  return width * 4 * 4;
}

struct uchar4 {
  uchar4() = default;
  uchar4(uint8_t x, uint8_t y, uint8_t z, uint8_t w) : x(x), y(y), z(z), w(w) {}
  uint8_t x, y, z, w;
  template <typename T>
  uchar4 operator+(T v) {
    return {x + v, y + v, z * v, w * v};
  }
  template <typename T>
  uchar4 operator-(T v) {
    return {x - v, y - v, z - v, w - v};
  }
  template <typename T>
  uchar4 operator*(T v) {
    return {x * v, y * v, z * v, w * v};
  }
  template <typename T>
  uchar4 operator/(T v) {
    return {x / v, y / v, z / v, w / v};
  }
};

struct ushort4 {
  ushort4() = default;
  ushort4(uint16_t x, uint16_t y, uint16_t z, uint16_t w) : x(x), y(y), z(z), w(w) {}
  uint16_t x, y, z, w;
  template <typename T>
  ushort4 operator+(T v) {
    return {x + v, y + v, z * v, w * v};
  }
  template <typename T>
  ushort4 operator-(T v) {
    return {x - v, y - v, z - v, w - v};
  }
  template <typename T>
  ushort4 operator*(T v) {
    return {x * v, y * v, z * v, w * v};
  }
  template <typename T>
  ushort4 operator/(T v) {
    return {x / v, y / v, z / v, w / v};
  }
};

struct float4 {
  float4() = default;
  float4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
  float x, y, z, w;
  template <typename T>
  float operator+(T v) {
    return {x + v, y + v, z * v, w * v};
  }
  template <typename T>
  float operator-(T v) {
    return {x - v, y - v, z - v, w - v};
  }
  template <typename T>
  float operator*(T v) {
    return {x * v, y * v, z * v, w * v};
  }
  template <typename T>
  float operator/(T v) {
    return {x / v, y / v, z / v, w / v};
  }
};

template <typename T>
inline void resizeNN(const T* src, T* dst, int sw, int sh, int dw, int dh) {
  for (int y = 0; y < dh; ++y) {
    for (int x = 0; x < dw; ++x) {
      int sy = (int)((float)y / dh * sh);
      int sx = (int)((float)x / dw * sw);
      dst[y * dw + x] = src[sy * sw + sx];
    }
  }
}

template <typename T>
inline void resizeBilinear(
    const T* src, T* dst, int sw, int sh, int dw, int dh) {
  for (int y = 0; y < dh; ++y) {
    for (int x = 0; x < dw; ++x) {
      float fy = (float)y / dh * sh;
      float fx = (float)x / dw * sw;
      int gy = (int)fy;
      int gx = (int)fx;

      const auto p = [src, sw, sh](int x, int y) {
        x = std::min(sw - 1, x);
        y = std::min(sh - 1, y);
        return src[y * sw + x];
      };

      T c00 = p(gx, gy);
      T c10 = p(gx + 1, gy);
      T c01 = p(gx, gy + 1);
      T c11 = p(gx + 1, gy + 1);

      const auto lerp = [](const T& s, const T& e, float t) {
        return s + (e - s) * t;
      };
      T ret = lerp(lerp(c00, c10, gx), lerp(c01, c11, gx), gy);

      dst[y * dw + x] = ret;
    }
  }
}

bool Image::IsSupported(const std::string& ext) {
  return WicRW::IsSupported(ext) || StbRW::IsSupported(ext) ||
         PnmRW::IsSupported(ext);
}

const std::vector<std::string>& Image::GetAllSupportedExtensions() {
  static std::vector<std::string> exts;
  if (exts.empty()) {
    exts.insert(exts.end(), StbRW::kSupportedExtensions.begin(), StbRW::kSupportedExtensions.end()); 
    exts.insert(exts.end(), WicRW::kSupportedExtensions.begin(), WicRW::kSupportedExtensions.end()); 
    exts.insert(exts.end(), PnmRW::kSupportedExtensions.begin(), PnmRW::kSupportedExtensions.end()); 
  }
  return exts;
}

std::unique_ptr<Image> Image::Load(const std::string& path, int pos) {
  std::unique_ptr<ImageRW> reader = CreateImageRW(path);
  if (!reader) {
    return nullptr;
  }
  return reader->Read(path);
}

std::vector<uint8_t> Image::Save(
    const Image* image, const std::string& format) {
  StbRW stbrw;
  std::vector<uint8_t> data = stbrw.Write(image, format);
  if (data.empty()) {
    return {};
  }
  return data;
}

Image::Image() {}

Image::Image(int width, int height, size_t stride, PixelFormat format,
    int channels, ColorSpace cs, data_t&& data)
    : width_(width),
      height_(height),
      stride_(stride),
      format_(format),
      channels_(channels),
      cs_(cs),
      data_(std::make_shared<data_t>(std::move(data))){}

std::unique_ptr<Image> Image::Clone() const {
  return std::unique_ptr<Image>(new Image(*this));
}

std::unique_ptr<Image> Image::Convert(PixelFormat target) const {
  // TODO:
  assert(false && "not implemented.");
  return {};
}

bool Image::Extract(int x, int y, Color& color) const {
  if (x < 0 || x >= width()) return false;
  if (y < 0 || y >= height()) return false;

  if (format_ == PixelFormat::RGBA8) {
    const uchar4& rgba = *((uchar4*)data() + y * width_ + x);
    color = Color(rgba.x, rgba.y, rgba.z, rgba.w);
  } else if (format_ == PixelFormat::RGBA16) {
    const ushort4& rgba = *((ushort4*)data() + y * width_ + x);
    color = Color(rgba.x / 65535.0f, rgba.y / 65535.0f, rgba.z / 65535.0f,
        rgba.w / 65535.0f);
  } else if (format_ == PixelFormat::RGBA32F) {
    const float4& rgba = *((float4*)data() + y * width_ + x);
    color = Color(rgba.x, rgba.y, rgba.z, rgba.w);
  } else {
    assert(false && "format not implemented.");
    return false;
  }

  return true;
}

std::unique_ptr<Image> Image::Resize(
    int dst_width, int dst_height, ResizeFilter filter) const {
  data_t buf(getPitch(format_, dst_width) * dst_height);

  size_t dst_stride = 0;
  if (format_ == PixelFormat::RGBA8) {
    resizeNN<uchar4>(reinterpret_cast<const uchar4*>(data()),
        reinterpret_cast<uchar4*>(buf.data()), width_, height_, dst_width,
        dst_height);
    dst_stride = dst_width * sizeof(uchar4);
  } else if (format_ == PixelFormat::RGBA16) {
    resizeNN<ushort4>(reinterpret_cast<const ushort4*>(data()),
        reinterpret_cast<ushort4*>(buf.data()), width_, height_, dst_width,
        dst_height);
    dst_stride = dst_width * sizeof(ushort4);
  } else if (format_ == PixelFormat::RGBA32F) {
    resizeNN<float4>(reinterpret_cast<const float4*>(data()),
        reinterpret_cast<float4*>(buf.data()), width_, height_, dst_width,
        dst_height);
    dst_stride = dst_width * sizeof(float4);
  }

  std::unique_ptr<Image> dst = Clone();
  dst->width_ = dst_width;
  dst->height_ = dst_height;
  dst->stride_ = dst_stride;
  dst->data_ = std::make_shared<data_t>(std::move(buf));
  return dst;
}

Image::~Image() {}

std::unique_ptr<ImageRW> CreateImageRW(const std::string& path) {
  std::filesystem::path fspath(path);
  const std::string& ext = fspath.extension().string();

  if (WinRTRW::IsSupported(ext)) {
    try {
      return std::unique_ptr<ImageRW>(new WinRTRW());
    } catch (std::exception& ex) {
      LOG_F(DEBUG, "failed WinRTRW::Create() %s", ex.what());
    }
  }

#if 0
  if (WicRW::IsSupported(ext)) {
    try {
      return std::unique_ptr<ImageRW>(new WicRW());
    } catch (std::exception& ex) {
      LOG_F(DEBUG, "failed WicRW::Create() %s", ex.what());
    }
  }
#endif

  if (StbRW::IsSupported(ext)) {
    try {
      return std::unique_ptr<ImageRW>(new StbRW());
    } catch (std::exception& ex) {
      LOG_F(DEBUG, "failed StbRW::Create() %s", ex.what());
    }
  }

  if (PnmRW::IsSupported(ext)) {
    try {
      return std::unique_ptr<ImageRW>(new PnmRW());
    } catch (std::exception& ex) {
      LOG_F(DEBUG, "failed PnmRW::Create() %s", ex.what());
    }
  }

  return nullptr;
}

ImageRW::~ImageRW() {}

}  // namespace chaos
