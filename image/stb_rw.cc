#include "stb_rw.h"

#include "base/fs.h"
#include "base/text.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define __STDC_LIB_EXT1__
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

constexpr size_t kHeaderSize = 8192;

namespace chaos {

DEFINE_IMAGE_RW_EXTENSIONS(StbRW, ".jpg", ".jpeg", ".tga", ".png", ".bmp",
    ".psd", ".gif", ".hdr", ".pic", ".pnm")

StbRW::StbRW() {}

StbRW::~StbRW() {}

std::unique_ptr<Image> StbRW::Read(const std::string& path, int pos,
    int prefer_width, int prefer_height, bool only_header) {
  if (pos > 0) {
    assert(false && "not supported yet.");
    return nullptr;
  }

  // Decode header
  int x, y, comp;
  {
    std::unique_ptr<FileStream> stream(new FileStream(path));
    size_t header_size = std::min(kHeaderSize, stream->Size());
    std::vector<uint8_t> buf(header_size);
    stream->Read(buf.data(), header_size);
    stbi_info_from_memory(buf.data(), (int)buf.size(), &x, &y, &comp);

    if (only_header) {
      std::unique_ptr<Image> image(new Image(
          x, y, 0, chaos::PixelFormat::Unknown, comp, chaos::ColorSpace::sRGB));
      return image;
    }
  }

  // Decode data
  std::unique_ptr<FileStream> stream(new FileStream(path));

  std::vector<uint8_t> buf(stream->Size());
  stream->Read(buf.data(), stream->Size());

  void* data = nullptr;
  PixelFormat format;
  ColorSpace cs;
  size_t stride;

  int is_16bit = stbi_is_16_bit_from_memory(buf.data(), (int)buf.size());
  int is_hdr = stbi_is_hdr_from_memory(buf.data(), (int)buf.size());
  if (is_16bit) {
    data = stbi_load_16_from_memory(buf.data(), (int)buf.size(), &x, &y, &comp, 4);
    stride = x * 4 * 2;
    format = PixelFormat::RGBA16;
    cs = ColorSpace::sRGB;
  } else {
    static_assert(sizeof(stbi_uc) == sizeof(uint8_t),
                  "sizeof(stbi_uc) == sizeof(uint8_t).");
    data = stbi_load_from_memory(buf.data(), (int)buf.size(), &x, &y, &comp, 4);
    format = PixelFormat::RGBA8;
    cs = ColorSpace::sRGB;
    stride = x * 4;
  }

  if (!data) {
    return nullptr;
  }

  const size_t size = stride * y;
  std::vector<uint8_t> buffer(size);
  ::memcpy(buffer.data(), data, size);
  stbi_image_free(data);

  std::unique_ptr<Image> image(new Image(x, y, stride, format, 3, cs, std::move(buffer)));
  return image;
}

std::vector<uint8_t> StbRW::Write(
    const Image* image, const std::string& format) {
  std::vector<uint8_t> buf;

  if (format == "png") {
    std::unique_ptr<Image> converted = image->Convert(PixelFormat::RGBA8);

    stbi_write_png("e:\\test.png", converted->width(), converted->height(),
        getPixelFormatChannels(converted->format()), converted->data(),
        (int)converted->stride());

    int ret = stbi_write_png_to_func(
        [](void* ctx, void* data, int size) {
          std::vector<uint8_t>* buf = (std::vector<uint8_t>*)ctx;
          buf->resize(size);
          ::memcpy(buf->data(), data, size);
        },
        &buf, converted->width(), converted->height(),
        getPixelFormatChannels(converted->format()), converted->data(),
        (int)converted->stride());
    if (ret) {
      return std::move(buf);
    }
  }

  return {};
}

}  // namespace chaos
