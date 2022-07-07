#pragma once

#include <cassert>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string_view>

#include "../graphics/imgui/imgui.h"

namespace chaos {

constexpr uint64_t kImDrawCallbackSetDrawOutline = -100;
constexpr uint64_t kImDrawCallbackSetTextureFilter = -101;
constexpr uint64_t kImDrawCallbackSetSrcColorSpace = -102;
constexpr uint64_t kImDrawCallbackSetDstColorSpace = -103;

enum class ColorSpace {
  sRGB,
  Linear,
};

enum class PixelFormat {
  RGBA8,
  RGBA16,
  RGBA32F,
  BGRA8,
  Unknown,
};

enum class TextureFilter {
  Nearest = 0,
  Bilinear = 1,
};

inline int getPixelFormatChannels(PixelFormat format) {
  switch (format) {
    case PixelFormat::RGBA8:
    case PixelFormat::RGBA32F:
    case PixelFormat::BGRA8:
      return 4;
    default:
      assert(false && "unknown format.");
      throw std::runtime_error("unknown format.");
  }
}

class Color {
 public:
  Color() : r_(), g_(), b_(), a_() {}
  Color(const std::string& rgba_hex) {
    if (rgba_hex.empty()) {
      assert(false && "string is empty.");
      return;
    }

    if (rgba_hex.size() != 7 && rgba_hex.size() != 9) {
      assert(false && "unexpected length.");
      return;
    }

    if (rgba_hex[0] != '#') {
      assert(false && "unexpected string.");
      return;
    }

    r_ = std::stol(rgba_hex.substr(1, 2), nullptr, 16) / 255.0f;
    g_ = std::stol(rgba_hex.substr(3, 2), nullptr, 16) / 255.0f;
    b_ = std::stol(rgba_hex.substr(5, 2), nullptr, 16) / 255.0f;
    if (rgba_hex.size() == 9) {
      a_ = std::stoul(rgba_hex.substr(7, 2), nullptr, 16) / 255.0f;
    } else {
      a_ = 1.0f;
    }
  }
  Color(uint32_t rgba_hex) {
    r_ = ((rgba_hex >> 24) & 0xff) / 255.0f;
    g_ = ((rgba_hex >> 16) & 0xff) / 255.0f;
    b_ = ((rgba_hex >> 8) & 0xff) / 255.0f;
    a_ = ((rgba_hex)&0xff) / 255.0f;
  }
  Color(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
      : r_(r / 255.0f), g_(g / 255.0f), b_(b / 255.0f), a_(a / 255.0f) {}
  Color(float r, float g, float b, float a = 1.0f)
      : r_(r), g_(g), b_(b), a_(a) {}

  operator ImVec4() const { return {r_, g_, b_, a_}; }

  Color linear_to_srgb() const {
    auto convert = [](float in) {
      if (in <= 0.0031308f) {
        return in * 12.92f;
      } else {
        return 1.055f * std::powf(in, 1.0f / 2.4f) - 0.055f;
      }
    };

    float r = convert(r_);
    float g = convert(g_);
    float b = convert(b_);
    return Color(r, g, b, a_);
  }
  Color srgb_to_linear() const {
    auto convert = [](float in) {
      if (in <= 0.04045f) {
        return in / 12.92f;
      } else {
        return std::powf((in + 0.055f) / 1.055f, 2.4f);
      }
    };

    float r = convert(r_);
    float g = convert(g_);
    float b = convert(b_);
    return Color(r, g, b, a_);
  }

  std::string hex(int channels = 4) const {
    std::stringstream ss;
    ss << "#";
    ss << std::setfill('0') << std::setw(2) << std::right << std::uppercase
       << std::hex << (int)(r_ * 255.0f + 0.5f);
    ss << std::setfill('0') << std::setw(2) << std::right << std::uppercase
       << std::hex << (int)(g_ * 255.0f + 0.5f);
    ss << std::setfill('0') << std::setw(2) << std::right << std::uppercase
       << std::hex << (int)(b_ * 255.0f + 0.5f);
    if (channels > 3) {
      ss << std::setfill('0') << std::setw(2) << std::right << std::uppercase
         << std::hex << (int)(a_ * 255.0f + 0.5f);
    }
    return ss.str();
  }
  std::string str(int channels = 4) const {
    std::stringstream ss;
    ss << "rgb(" << (int)(r_ * 255.0f + 0.5f);
    ss << ", " << (int)(g_ * 255.0f + 0.5f);
    ss << ", " << (int)(b_ * 255.0f + 0.5f);
    if (channels > 3) {
      ss << ", " << (int)(a_ * 255.0f + 0.5f) << ")";
    } else {
      ss << ")";
    }
    return ss.str();
  }
  uint32_t rgba8() const {
    uint32_t r = std::clamp((uint32_t)std::round(r_ * 255.0f), 0u, 255u);
    uint32_t g = std::clamp((uint32_t)std::round(g_ * 255.0f), 0u, 255u);
    uint32_t b = std::clamp((uint32_t)std::round(b_ * 255.0f), 0u, 255u);
    uint32_t a = std::clamp((uint32_t)std::round(a_ * 255.0f), 0u, 255u);
    return (r << 24) | (g << 16) | (b << 8) | a;
  }

 private:
  float r_, g_, b_, a_;
};

}  // namespace chaos
