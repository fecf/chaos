#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#define WIN32_EXTRA_LEAN
#define NOIME
#define NOMINMAX

#include <stdexcept>

#include <d3d12.h>
#include <dxcapi.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <d3dx12.h>

#include <wrl.h>
using namespace Microsoft::WRL;

#include <D3D12MemAlloc.h>

namespace chaos {

// Helper class for COM exceptions
class com_exception : public std::exception {
 public:
  com_exception(HRESULT hr) noexcept : result(hr) {}
  const char* what() const noexcept override {
    static char s_str[64] = {};
    sprintf_s(s_str, "Failure with HRESULT of %08X",
        static_cast<unsigned int>(result));
    return s_str;
  }
  HRESULT get_result() const noexcept { return result; }

 private:
  HRESULT result;
};

// Helper utility converts D3D API failures into exceptions.
inline void ThrowIfFailed(HRESULT hr) noexcept(false) {
  if (FAILED(hr)) {
    throw com_exception(hr);
  }
}

std::string D3D12GetName(ID3D12Object* object);

}  // namespace chaos
