#pragma once

#include <array>
#include <stdexcept>
#include <string>

// Slim version of windows.h header
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef WIN32_EXTRA_LEAN
#define WIN32_EXTRA_LEAN
#endif

#define NOIME
// #define NOWINRES
// #define NOGDICAPMASKS
// #define NOVIRTUALKEYCODES
// #define NOWINMESSAGES
// #define NOWINSTYLES
// #define NOSYSMETRICS
// #define NOMENUS
// #define NOICONS
// #define NOKEYSTATES
// #define NOSYSCOMMANDS
// #define NORASTEROPS
// #define NOSHOWWINDOW
// #define OEMRESOURCE
// #define NOATOM
// #define NOCLIPBOARD
// #define NOCOLOR
// #define NOCTLMGR
// #define NODRAWTEXT
// #define NOGDI
// #define NOUSER
// #define NOMB
// #define NOMEMMGR
// #define NOMETAFILE
#define NOMINMAX
// #define NOMSG
// #define NOOPENFILE
// #define NOSCROLL
// #define NOSERVICE
// #define NOSOUND
// #define NOTEXTMETRIC
// #define NOWH
// #define NOWINOFFSETS
// #define NOCOMM
// #define NOKANJI
// #define NOHELP
// #define NOPROFILER
// #define NODEFERWINDOWPOS
// #define NOMCX
// #define NOIME
// #define NOPROXYSTUB
// #define NOIMAGE
// #define NO
// #define NOTAPE
// #define ANSI_ONLY

#include <oleidl.h>
#include <shellapi.h>
#include <windows.h>
#include <wrl.h>
using namespace Microsoft::WRL;

namespace chaos {

inline std::string error_message(DWORD id) {
#if 0
  if (id != 0) {
    return std::system_category().message(id);
  } else {
    return {};
  }
#else
  std::string buf(1024, 0);
  DWORD ret = ::FormatMessage(
      FORMAT_MESSAGE_FROM_SYSTEM, nullptr, id, 0, buf.data(), 1024, nullptr);
  if (ret < 0) {
    DWORD error = ::GetLastError();
    throw std::runtime_error(std::to_string(error));
  }

  return buf;
#endif
}

inline std::string createGUID() {
  GUID guid;
  HRESULT hr = ::CoCreateGuid(&guid);
  if (FAILED(hr)) {
    throw std::runtime_error("falied CoCreateGuid().");
  }

  std::string result{};
  std::array<char, 36> buffer{};
  ::snprintf(buffer.data(), buffer.size(),
      "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x", guid.Data1,
      guid.Data2, guid.Data3, guid.Data4[0], guid.Data4[1], guid.Data4[2],
      guid.Data4[3], guid.Data4[4], guid.Data4[5], guid.Data4[6],
      guid.Data4[7]);
  result = std::string(buffer.data());
  return result;
}

struct ScopedCoInitialize {
  ScopedCoInitialize() : uninit(false) {
    HRESULT hr;
    hr = ::CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (hr == FALSE) {
      // Already initialized in current thread.
      return;
    }
    uninit = true;
  }

  ~ScopedCoInitialize() {
    if (uninit) {
      ::CoUninitialize();
    }
  }

  bool uninit;
};

}  // namespace chaos
