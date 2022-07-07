#pragma once

#include <string>
#include <cstdint>

namespace chaos {

namespace str {

inline std::string from_u8string(const std::string& s) { return s; }
inline std::string from_u8string(std::string&& s) { return std::move(s); }
#if defined(__cpp_lib_char8_t)
inline std::string from_u8string(const std::u8string& s) {
  return std::string(s.begin(), s.end());
}
#endif

std::string narrow(const std::wstring& wstr);
std::wstring widen(const std::string& str);
std::string to_utf8(const std::wstring& str);
std::string to_utf8(const std::string& str);
std::wstring utf8_to_utf16(const std::string& str);
std::string utf16_to_utf8(const std::wstring& str);
std::string last_segment(const std::string& path);
std::string to_lower(const std::string& str);
std::string to_upper(const std::string& str);
std::string readable_byte_count(size_t bytes, bool si);
bool compare_natural(const std::string& lhs, const std::string& rhs);
std::string timepoint_to_string(const std::chrono::system_clock::time_point& t);

template <typename T>
auto converter(const T& v) {
  return v;
}
inline const char* converter(const std::string& v) { return v.c_str(); }

template <typename... Args>
std::string format(const std::string& format, Args&&... args) {
  int size_s =
      std::snprintf(nullptr, 0, format.c_str(), converter(args)...) + 1;
  if (size_s <= 0) {
    return {};
  }

  thread_local std::vector<char> buf;
  if (buf.size() < (size_t)size_s) buf.resize(size_s * 2, '\0');

  std::snprintf(buf.data(), size_s, format.c_str(), args...);
  return std::string(buf.data(), buf.data() + size_s - 1);
}

}  // namespace str

}  // namespace chaos
