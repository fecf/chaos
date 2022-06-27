#include "wic_rw.h"

#include "base/text.h"
#include "base/fs.h"
#include "base/minlog.h"

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "shlwapi.lib")
#include <Shlwapi.h>
#include <comdef.h>
#include <wincodecsdk.h>

void throw_com_error(HRESULT hr, const char* file, int line) {
  if (FAILED(hr)) {
    _com_error err(hr);
    std::string str = chaos::str::from_u8string(err.ErrorMessage());
    ::OutputDebugStringW(chaos::str::widen(err.ErrorMessage()).c_str());
    ::OutputDebugStringW(L"\n");
    throw std::runtime_error(str);
  }
}

#define CHECK(hr) throw_com_error(hr, __FILE__, __LINE__)

using namespace Microsoft::WRL;

namespace chaos {

DEFINE_IMAGE_RW_EXTENSIONS(WicRW, ".jpg", ".jpeg", ".tif", ".tiff", ".gif",
    ".png", ".bmp", ".jxr", ".ico");

WicRW::WicRW() {}

WicRW::~WicRW() {}

std::unique_ptr<Image> WicRW::Read(const std::string& path, int pos,
    int prefer_width, int prefer_height, bool header_only) {
  try {
    ScopedCoInitialize coinit;

    ComPtr<IStream> filestream;
    CHECK(::SHCreateStreamOnFileEx(str::utf8_to_utf16(path).c_str(),
        STGM_READ | STGM_SHARE_DENY_NONE, 0, FALSE, NULL, &filestream));

    ComPtr<IWICImagingFactory2> factory;
    CHECK(::CoCreateInstance(CLSID_WICImagingFactory2, NULL,
        CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)));

    ComPtr<IWICStream> stream;
    CHECK(factory->CreateStream(stream.GetAddressOf()));
    CHECK(filestream->Seek({}, STREAM_SEEK_SET, NULL));
    CHECK(stream->InitializeFromIStream(filestream.Get()));

    ComPtr<IWICBitmapDecoder> decoder;
    CHECK(factory->CreateDecoderFromStream(
        stream.Get(), NULL, WICDecodeMetadataCacheOnLoad, &decoder));

    UINT frame_count;
    CHECK(decoder->GetFrameCount(&frame_count));
    if (frame_count == 0) {
      return {};
    }

    ComPtr<IWICBitmapFrameDecode> bitmap_frame;
    CHECK(decoder->GetFrame(pos, &bitmap_frame));

    // get channels, bit depth
    WICPixelFormatGUID pixel_format_guid;
    CHECK(bitmap_frame->GetPixelFormat(&pixel_format_guid));
    ComPtr<IWICComponentInfo> component_info;
    CHECK(factory->CreateComponentInfo(pixel_format_guid, &component_info));
    ComPtr<IWICPixelFormatInfo2> pixel_format_info;
    CHECK(component_info.As(&pixel_format_info));

    UINT channels, bpp;
    CHECK(pixel_format_info->GetChannelCount(&channels));
    CHECK(pixel_format_info->GetBitsPerPixel(&bpp));

    PixelFormat format = PixelFormat::RGBA8;
    ColorSpace cs = ColorSpace::sRGB;

    ComPtr<IWICFormatConverter> converter;
    CHECK(factory->CreateFormatConverter(&converter));
    if (bpp > 32) {
      CHECK(converter->Initialize(bitmap_frame.Get(),
          GUID_WICPixelFormat128bppRGBAFloat, WICBitmapDitherTypeNone, nullptr,
          0.0f, WICBitmapPaletteTypeCustom));
      format = PixelFormat::RGBA32F;
      cs = ColorSpace::Linear;
    } else {
      CHECK(converter->Initialize(bitmap_frame.Get(),
          GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr,
          0.0f, WICBitmapPaletteTypeCustom));
    }

    ComPtr<IWICBitmap> bitmap;
    CHECK(factory->CreateBitmapFromSource(converter.Get(), WICBitmapNoCache, &bitmap));

    ComPtr<IWICBitmapLock> lock;
    CHECK(bitmap->Lock(NULL, WICBitmapLockRead, &lock));

    UINT w, h;
    CHECK(lock->GetSize(&w, &h));

    UINT stride;
    CHECK(lock->GetStride(&stride));

    UINT bufsize;
    WICInProcPointer ptr;
    CHECK(lock->GetDataPointer(&bufsize, &ptr));

    std::vector<uint8_t> buffer(stride * h);
    ::memcpy_s(buffer.data(), bufsize, ptr, bufsize);

    return std::unique_ptr<Image>(
        new Image(w, h, stride, format, channels, cs, std::move(buffer)));
  } catch (std::exception&) {
    return {};
  }
}

}  // namespace chaos
