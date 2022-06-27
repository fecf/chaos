#pragma once

#include "image.h"

// STL
#include <memory>
#include <string>

namespace chaos {

class WinRTRW : public ImageRW {
 public:
  DECLARE_IMAGE_RW;

  WinRTRW();
  virtual ~WinRTRW();

  virtual std::unique_ptr<Image> Read(const std::string& path, int pos,
      int prefer_width, int prefer_height, bool header_only) override;

 private:
  std::string path_;
};

}  // namespace chaos
