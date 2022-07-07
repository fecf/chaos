#include "winrt_rw.h"

#include "base/text.h"
#include "base/fs.h"
#include "base/minlog.h"

#include <optional>

#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <wrl/client.h>

MIDL_INTERFACE("5b0d3235-4dba-4d44-865e-8f1d0e4fd04d")
IMemoryBufferByteAccess : ::IUnknown
{
    virtual HRESULT __stdcall GetBuffer(BYTE **value, UINT32 *capacity) = 0;
};

namespace chaos {

DEFINE_IMAGE_RW_EXTENSIONS(WinRTRW, ".jpg", ".jpeg", ".tif", ".tiff", ".gif",
    ".png", ".bmp", ".jxr", ".ico");

WinRTRW::WinRTRW() {}

WinRTRW::~WinRTRW() {}

std::unique_ptr<Image> WinRTRW::Read(const std::string& path, int pos,
    int prefer_width, int prefer_height, bool header_only) {
  int w, h, stride;
  std::vector<uint8_t> buffer;

  std::exception_ptr exptr = nullptr;
  std::thread th([&] {
    try {
      using namespace winrt::Windows::Foundation;
      using namespace winrt::Windows::Storage;
      using namespace winrt::Windows::Storage::Streams;
      using namespace winrt::Windows::Graphics::Imaging;

      StorageFile sf =
          StorageFile::GetFileFromPathAsync(winrt::to_hstring(path)).get();
      IRandomAccessStream stream = sf.OpenAsync(FileAccessMode::Read).get();

      BitmapDecoder decoder = BitmapDecoder::CreateAsync(stream).get();
      SoftwareBitmap software_bitmap = decoder.GetSoftwareBitmapAsync().get();
      if (software_bitmap.BitmapPixelFormat() != BitmapPixelFormat::Rgba8 ||
          software_bitmap.BitmapAlphaMode() == BitmapAlphaMode::Straight) {
        software_bitmap = SoftwareBitmap::Convert(software_bitmap,
            BitmapPixelFormat::Rgba8, BitmapAlphaMode::Premultiplied);
      }

      w = software_bitmap.PixelWidth();
      h = software_bitmap.PixelHeight();
      stride = w * 4;
      BitmapBuffer buf =
          software_bitmap.LockBuffer(BitmapBufferAccessMode::Read);
      ComPtr<IMemoryBufferByteAccess> access =
          buf.CreateReference().as<IMemoryBufferByteAccess>().get();
      UINT32 capacity = 0;
      BYTE* bytes = nullptr;
      access->GetBuffer(&bytes, &capacity);

      buffer.resize(capacity);
      ::memcpy(buffer.data(), bytes, capacity);
    } catch (...) {
      exptr = std::current_exception();
    }
  });
  th.join();

  if (exptr) {
    return nullptr;
  }

  return std::unique_ptr<Image>(new Image(w, h, stride, PixelFormat::RGBA8, 4, ColorSpace::sRGB, std::move(buffer)));
}

}  // namespace chaos
