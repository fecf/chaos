#pragma once

#include "image.h"

// STL
#include <memory>
#include <string>

namespace chaos {

class WicRW : public ImageRW {
 public:
  DECLARE_IMAGE_RW;

  WicRW();
  virtual ~WicRW();

  virtual std::unique_ptr<Image> Read(const std::string& path, int pos,
      int prefer_width, int prefer_height, bool header_only) override;

 private:
  std::string path_;
};

}  // namespace chaos
