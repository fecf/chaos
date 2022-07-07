#include "d3d12def.h"

namespace chaos {

std::string D3D12GetName(ID3D12Object* object) {
  HRESULT hr;
  UINT size;

  hr = object->GetPrivateData(WKPDID_D3DDebugObjectName, &size, nullptr);
  if (SUCCEEDED(hr)) {
    std::string data(size, '\0');
    hr = object->GetPrivateData(WKPDID_D3DDebugObjectName, &size, data.data());
    if (SUCCEEDED(hr)) {
      data.shrink_to_fit();
      return data;
    }
  }

  hr = object->GetPrivateData(WKPDID_D3DDebugObjectNameW, &size, nullptr);
  if (SUCCEEDED(hr)) {
    std::wstring data(size, L'\0');
    hr = object->GetPrivateData(WKPDID_D3DDebugObjectNameW, &size, data.data());
    if (SUCCEEDED(hr)) {
      return std::filesystem::path(data).string();
    }
  }

  hr = object->GetPrivateData(WKPDID_D3DAutoDebugObjectNameW, &size, nullptr);
  if (SUCCEEDED(hr)) {
    std::wstring data(size, L'\0');
    hr = object->GetPrivateData(
        WKPDID_D3DAutoDebugObjectNameW, &size, data.data());
    if (SUCCEEDED(hr)) {
      return std::filesystem::path(data).string();
    }
  }

  return "";
}

}  // namespace chaos
