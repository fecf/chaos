#pragma once

#include "image.h"

#include <memory>
#include <string>

namespace chaos {

class FileStream;

class PnmRW : public ImageRW {
 public:
  DECLARE_IMAGE_RW;

  PnmRW();
  virtual ~PnmRW();

  virtual std::unique_ptr<Image> Read(const std::string& path, int pos,
      int prefer_width, int prefer_height, bool header_only) override;

 private:
  std::string path_;
  std::unique_ptr<FileStream> stream_;
};

}  // namespace chaos
