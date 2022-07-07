#pragma once

#include <memory>
#include <string>

namespace chaos {

class Shader {
 public:
  virtual ~Shader();

  virtual const void* GetBytecode() = 0;
  virtual size_t GetSize() = 0;
};

std::unique_ptr<Shader> CreateShaderFromBytecode(
    const void* bytecode, size_t size);
std::unique_ptr<Shader> CreateShaderFromString(const std::string& code,
    const std::string& entrypoint, const std::string& target);

}  // namespace chaos
