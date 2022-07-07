#pragma once

#include <memory>
#include <string>
#include <vector>

#include "../base/types.h"

#define DECLARE_IMAGE_RW                       \
  static bool IsSupported(const std::string& ext); \
  static std::vector<std::string> kSupportedExtensions;

#define DEFINE_IMAGE_RW_EXTENSIONS(_class, ...)               \
  std::vector<std::string> _class::kSupportedExtensions = {__VA_ARGS__}; \
  bool _class## ::IsSupported(const std::string& ext) {                     \
    auto it = std::find(kSupportedExtensions.begin(),                       \
        kSupportedExtensions.end(), str::to_lower(ext));                    \
    return it != kSupportedExtensions.end();                                \
  }

namespace chaos {

enum class ResizeFilter { Nearest = 0, Bilinear = 1};

class Image;
class ImageRW;

std::unique_ptr<ImageRW> CreateImageRW(const std::string& url);

class ImageRW {
 public:
  virtual ~ImageRW();
  virtual std::unique_ptr<Image> Read(const std::string& path, int pos = 0,
      int prefer_width = 0, int prefer_height = 0, bool header_only = false) {
    throw std::domain_error("not implemented.");
  };
  virtual std::vector<uint8_t> Write(const Image* image, const std::string& format) {
    throw std::domain_error("not implemented.");
  };
};

class Image {
  friend class ImageRW;
  using data_t = std::vector<uint8_t>;

 private:
  Image();

 public:
  static bool IsSupported(const std::string& ext);
  static const std::vector<std::string>& GetAllSupportedExtensions();
  static std::unique_ptr<Image> Load(const std::string& path, int pos = 0);
  static std::vector<uint8_t> Save(const Image* image, const std::string& format);

  Image(const Image&) = default;
  Image& operator=(const Image&) = default;
  Image(int width, int height, size_t stride, PixelFormat format, int channels,
      ColorSpace cs, data_t&& data = {});
  ~Image();

  int width() const noexcept { return width_; }
  int height() const noexcept { return height_; }
  size_t stride() const noexcept { return stride_; }
  PixelFormat format() const noexcept { return format_; }
  int channels() const noexcept { return channels_; }
  ColorSpace colorspace() const noexcept { return cs_; }
  const uint8_t* data() const noexcept { return data_->data(); };
  const std::vector<uint8_t>& data_vec() const noexcept { return *data_; }
  size_t size() const noexcept { return data_->size(); }

  std::unique_ptr<Image> Clone() const;
  std::unique_ptr<Image> Convert(PixelFormat target) const;
  bool Extract(int x, int y, Color& color) const;
  std::unique_ptr<Image> Resize(int width, int height, ResizeFilter filter) const;

 private:
  int width_;
  int height_;
  size_t stride_;
  PixelFormat format_;
  int channels_;
  ColorSpace cs_;
  std::shared_ptr<data_t> data_;
};

}  // namespace chaos
