#include "pnm_rw.h"
#include "image.h"

#include "base/fs.h"
#include "base/text.h"

#include <bitset>
#include <cassert>
#include <fstream>
#include <iosfwd>

#include "pnm.hpp"

namespace chaos {

DEFINE_IMAGE_RW_EXTENSIONS(PnmRW, ".pnm", ".pgm", ".ppm")

PnmRW::PnmRW() {}

PnmRW::~PnmRW() {}

struct membuf : std::streambuf {
  membuf(char* base, std::ptrdiff_t n) : begin(base), end(base + n) {
    this->setg(base, base, base + n);
  }

  virtual pos_type seekoff(
      off_type off, std::ios_base::seekdir dir,
      std::ios_base::openmode which = std::ios_base::in) override {
    if (dir == std::ios_base::cur)
      gbump((int)off);
    else if (dir == std::ios_base::end)
      setg(begin, end + off, end);
    else if (dir == std::ios_base::beg)
      setg(begin, begin + off, end);

    return gptr() - eback();
  }

  virtual pos_type seekpos(std::streampos pos,
                           std::ios_base::openmode mode) override {
    return seekoff(pos - pos_type(off_type(0)), std::ios_base::beg, mode);
  }

  char *begin, *end;
};

std::unique_ptr<Image> PnmRW::Read(const std::string& path, int pos,
    int prefer_width, int prefer_height, bool header_only) {
  if (pos > 0) {
    assert(false && "not supported.");
    return nullptr;
  }

  std::unique_ptr<FileStream> stream(new FileStream(path));
  size_t size = stream->Size();
  std::vector<uint8_t> buf(size);
  stream->Read(buf.data(), size);

  membuf membuf((char*)buf.data(), buf.size());
  std::istream ifs(&membuf);

  PNM::Info info;
  std::vector<std::uint8_t> data;
  ifs >> PNM::load(data, info);
  int w = static_cast<int>(info.width());
  int h = static_cast<int>(info.height());
  int ch = static_cast<int>(info.channel());
  int depth = static_cast<int>(info.depth());
  int value_max = static_cast<int>(info.max());

  if (ch != 1 && ch != 3) {
    throw std::runtime_error("unexpected channel.");
  }
  if (depth != 1 && depth != 8) {
    throw std::runtime_error("unexpected bit depth.");
  }

  std::vector<uint8_t> buffer(w * 4 * h);
  const uint8_t* src = data.data();
  uint32_t* dst = (uint32_t*)buffer.data();

  if (depth == 8) {
    if (ch == 1) {
      for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
          uint8_t v = src[y * w + x];
          dst[y * w + x] = 255 | v << 16 | v << 8 | v;
        }
      }
    } else {
      for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
          uint8_t r = src[y * w * 3 + x * 3 + 0];
          uint8_t g = src[y * w * 3 + x * 3 + 1];
          uint8_t b = src[y * w * 3 + x * 3 + 2];
          dst[y * w + x] = 255 | b << 16 | g << 8 | r;
        }
      }
    }
  } else if (depth == 1) {
    const uint8_t* src = data.data();
    uint32_t* dst = (uint32_t*)buffer.data();
    if (ch == 1) {
      for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
          uint8_t v = src[y * w + x] * 255 / value_max;
          dst[y * w + x] = 255 << 24 | v << 16 | v << 8 | v;
        }
      }
    } else {
      for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
          uint8_t r = src[y * w * 3 + x * 3 + 0] * 255 / value_max;
          uint8_t g = src[y * w * 3 + x * 3 + 1] * 255 / value_max;
          uint8_t b = src[y * w * 3 + x * 3 + 2] * 255 / value_max;
          dst[y * w + x] = 255 << 24 | b << 16 | g << 8 | r;
        }
      }
    }
  } else {
    assert(false && "unexpected bit depth.");
    return nullptr;
  }

  std::unique_ptr<Image> image(
      new Image(w, h, w * 4, PixelFormat::RGBA8, 3, ColorSpace::sRGB, std::move(buffer)));
  return image;
}

}  // namespace chaos
