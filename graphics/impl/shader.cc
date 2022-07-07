#include "shader.h"

#include "d3d12def.h"

namespace chaos {

Shader::~Shader() = default;

class D3D12Shader : public Shader {
 public:
  D3D12Shader() = delete;
  D3D12Shader(const CD3DX12_SHADER_BYTECODE& bytecode) : bytecode_(bytecode) {}
  ~D3D12Shader() = default;

  virtual const void* GetBytecode() override {
    return (const void*)bytecode_.pShaderBytecode;
  }
  virtual size_t GetSize() override { return bytecode_.BytecodeLength; }

 private:
  CD3DX12_SHADER_BYTECODE bytecode_;
};

class D3D12CompiledShader : public Shader {
 public:
  D3D12CompiledShader() = delete;
  D3D12CompiledShader(ComPtr<ID3DBlob> blob) : blob_(blob) {}
  ~D3D12CompiledShader() = default;

  virtual const void* GetBytecode() override {
    return blob_->GetBufferPointer();
  }
  virtual size_t GetSize() override { return blob_->GetBufferSize(); }

 private:
  ComPtr<ID3DBlob> blob_;
};

std::unique_ptr<Shader> CreateShaderFromBytecode(
    const void* bytecode, size_t size) {
  CD3DX12_SHADER_BYTECODE sb(bytecode, size);
  return std::unique_ptr<D3D12Shader>(new D3D12Shader(sb));
}

std::unique_ptr<Shader> CreateShaderFromString(const std::string& code,
    const std::string& entrypoint, const std::string& target) {
  /*
  HRESULT hr;
  ComPtr<ID3DBlob> blob;
  ComPtr<ID3DBlob> err;
  hr = ::D3DCompile(code.c_str(), code.size(), NULL, NULL, NULL,
                    entrypoint.c_str(), target.c_str(), 0, 0, &blob, &err);
  if (FAILED(hr)) {
    if (err) {
      const char* buf = static_cast<const char*>(err->GetBufferPointer());
      ::OutputDebugStringA(buf);
    }
    return nullptr;
  }
  return std::unique_ptr<D3D12CompiledShader>(new D3D12CompiledShader(blob));
  */
  return nullptr;
}

}  // namespace chaos
