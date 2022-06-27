#pragma once

#include "image.h"

#include <string>
#include <memory>

namespace chaos {

class FileStream;

class StbRW : public ImageRW {
 public:
  DECLARE_IMAGE_RW;

  StbRW();
  virtual ~StbRW();

  virtual std::unique_ptr<Image> Read(const std::string& path, int pos,
      int prefer_width, int prefer_height, bool only_header) override;
  virtual std::vector<uint8_t> Write(const Image* image, const std::string& format) override;
};

}  // namespace chaos
