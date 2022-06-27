#include "text.h"

#include <algorithm>
#include <cmath>
#include <string>

// Windows
#include <base/win32def.h>
#include <Shlwapi.h>
#pragma comment(lib, "shlwapi.lib")

#include <winrt/base.h>

namespace chaos {

namespace str {

std::string narrow(const std::wstring& wstr) {
  return winrt::to_string(wstr);
  if (wstr.empty()) return {};

#ifdef _WIN32
  int needed =
      ::WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
  if (needed < 0) {
    throw std::runtime_error("failed ::WideCharToMultiByte (needed < 0)");
  }

  std::string buf(needed - 1, '\0');
  int size = ::WideCharToMultiByte(
      CP_ACP, 0, wstr.c_str(), -1, buf.data(), needed, NULL, NULL);
  if (size < 0) {
    throw std::runtime_error("failed ::WideCharToMultiByte (size < 0)");
  }
  return buf;
#else
  static_assert(false, "not implemented.");
#endif
}

std::wstring widen(const std::string& str) {
  if (str.empty()) return {};

#ifdef _WIN32
  int needed = MultiByteToWideChar(
      CP_ACP, 0, &str[0], static_cast<int>(str.size()), NULL, 0);
  if (needed < 0) {
    throw std::runtime_error("failed ::MultiByteToWideChar (needed < 0)");
  }

  std::wstring ret(needed, 0);
  int size = MultiByteToWideChar(CP_ACP, 0, &str[0],
      static_cast<int>(str.size()), &ret[0], static_cast<int>(needed));
  if (size < 0) {
    throw std::runtime_error("failed ::MultiByteToWideChar (size < 0)");
  }
  return ret;
#else
  static_assert(false, "not implemented.");
#endif
}

std::string to_utf8(const std::wstring& str) {
#ifdef _WIN32
  int needed = WideCharToMultiByte(CP_UTF8, 0, str.c_str(),
      static_cast<int>(str.size()), NULL, 0, NULL, NULL);
  if (needed < 0) {
    throw std::runtime_error("failed ::WideCharToMultiByte (needed < 0)");
  }

  std::string utf8(needed, 0);
  int size = WideCharToMultiByte(CP_UTF8, 0, str.c_str(),
      static_cast<int>(str.size()), utf8.data(), needed, NULL, NULL);
  if (size < 0) {
    throw std::runtime_error("failed ::WideCharToMultiByte (size < 0)");
  }
  return utf8;
#else
  static_assert(false, "not implemented.");
#endif
}

std::string to_utf8(const std::string& str) {
  std::wstring wstr = widen(str);
  return to_utf8(wstr);
}

std::wstring utf8_to_utf16(const std::string& str) { 
  return (std::wstring)winrt::to_hstring(str);
}

std::string utf16_to_utf8(const std::wstring& str) { 
  return winrt::to_string(str);
}

std::string last_segment(const std::string& path) {
  auto pos = path.find_last_of("\\/");
  if (pos == std::string::npos) {
    return path;
  }
  if (pos + 1 == path.size()) {
    return path.substr(0, pos);
  }
  return path.substr(pos + 1);
}

std::string to_lower(const std::string& str) {
  std::string buf(str.size(), '\0');
  std::transform(str.begin(), str.end(), buf.begin(),
      [](char c) { return std::tolower(c); });
  return buf;
}

std::string to_upper(const std::string& str) {
  std::string buf(str.size(), '\0');
  std::transform(str.begin(), str.end(), buf.begin(),
      [](char c) { return std::toupper(c); });
  return buf;
}

std::string readable_byte_count(size_t bytes, bool si) {
  int unit = si ? 1000 : 1024;
  if (bytes < unit) return std::to_string(bytes) + " B";
  unsigned int exp = (int)(std::log(bytes) / std::log(unit));
  std::string units = si ? "kMGTPE" : "KMGTPE";
  std::string pre = units[exp - 1] + (si ? "" : "i");

  char buf[1024];
  sprintf_s(buf, "%.1f %sB", bytes / std::pow(unit, exp), pre.c_str());
  return buf;
}

bool compare_natural(const std::string& lhs, const std::string& rhs) {
  if (lhs == rhs) {
    return 0;
  }

  return ::StrCmpLogicalW(widen(lhs).c_str(), widen(rhs).c_str()) < 1;
}

std::string timepoint_to_string(
    const std::chrono::system_clock::time_point& t) {
  time_t tt = std::chrono::system_clock::to_time_t(t);
  tm local_tm{};
  errno_t err = localtime_s(&local_tm, &tt);
  if (err != 0) {
    throw std::runtime_error("failed localtime_s().");
  }
  constexpr const char* fmt = "%04d-%02d-%02dT%02d:%02d:%02d";
  return format(fmt, local_tm.tm_year + 1900, local_tm.tm_mon + 1,
      local_tm.tm_mday, local_tm.tm_hour, local_tm.tm_min, local_tm.tm_sec);
}

}  // namespace str

}  // namespace chaos
