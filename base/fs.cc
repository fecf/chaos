#include "fs.h"
#include "fs.h"

#include <filesystem>
#include <ShlObj_core.h>

#include "base/text.h"
#include "base/win32def.h"
#include "minlog.h"

inline std::chrono::system_clock::time_point filetime_to_system_clock(
    LPFILETIME ft) {
  ULARGE_INTEGER ul;
  ul.HighPart = ft->dwHighDateTime;
  ul.LowPart = ft->dwLowDateTime;
  time_t secs = ul.QuadPart / 10000000ULL - 11644473600ULL;
  auto tp = std::chrono::system_clock::from_time_t(secs);
  return tp;
};

namespace chaos {

namespace comparer {

bool name(const DirEntry& lhs, const DirEntry& rhs, bool desc) {
  return desc ? str::compare_natural(rhs.name, lhs.name)
              : str::compare_natural(lhs.name, rhs.name);
}

bool size(const DirEntry& lhs, const DirEntry& rhs, bool desc) {
  return desc ? (lhs.size > rhs.size) : (lhs.size < rhs.size);
}

bool created(const DirEntry& lhs, const DirEntry& rhs, bool desc) {
  return desc ? (lhs.created > rhs.created)
              : (lhs.created < rhs.created);
}

bool modified(const DirEntry& lhs, const DirEntry& rhs, bool desc) {
  return desc ? (lhs.modified > rhs.modified)
              : (lhs.modified < rhs.modified);
}

}  // namespace comparer

std::string getCurrentDirectory() {
  std::string val;
  char filename[1024]{};
  ::GetModuleFileName(NULL, filename, 1024);

  std::filesystem::path fspath(filename);
  fspath = fspath.make_preferred().parent_path();
  return str::from_u8string(fspath.u8string());
}

std::string getUserDirectory() {
  char path[MAX_PATH]{};
  HRESULT hr =
      ::SHGetFolderPath(NULL, CSIDL_APPDATA | CSIDL_FLAG_CREATE, NULL, 0, path);
  if (FAILED(hr)) {
    return {};
  }

  std::filesystem::path fspath(path);
  fspath = fspath.make_preferred();
  return str::from_u8string(fspath.u8string());
}

std::string getFontDirectory() {
  char path[MAX_PATH]{};
  HRESULT hr =
      ::SHGetFolderPathA(NULL, CSIDL_FONTS, NULL, SHGFP_TYPE_DEFAULT, path);
  if (FAILED(hr)) {
    return {};
  }

  std::filesystem::path fspath(path);
  fspath = fspath.make_preferred();
  return str::from_u8string(fspath.u8string());
}

std::vector<std::string> getCommandLineArgs() {
  std::wstring commandline = ::GetCommandLineW();
  if (commandline.empty()) {
    return {};
  }

  int argc = 0;
  LPWSTR* argv = ::CommandLineToArgvW(commandline.c_str(), &argc);
  if (argc <= 0 || argv == NULL) {
    return {};
  }

  std::vector<std::string> args(argc);
  for (int i = 0; i < argc; ++i) {
    args[i] = str::narrow(argv[i]);
  }
  return args;
}

const std::string kNativeSeparator =
    str::to_utf8(std::wstring(1, std::filesystem::path::preferred_separator));

FileInfo::FileInfo(const std::string& path)
    : raw_path_(path),
      valid_(false),
      path_(),
      name_(),
      ordinal_(),
      size_(),
      flags_(),
      created_(),
      modified_(),
      sort_type_(SortType::Name),
      sort_desc_(false) {
  refresh();
}

FileInfo::FileInfo(const std::string& path, FileInfo&& reference)
    : raw_path_(path),
      valid_(false),
      path_(),
      name_(),
      ordinal_(),
      size_(),
      flags_(),
      created_(),
      modified_() {
  refresh(std::move(reference));
}

FileInfo::FileInfo() : valid_(false) {}

bool FileInfo::refresh(FileInfo&& reference) {
  valid_ = false;

  std::error_code ec;
  std::filesystem::path fspath(raw_path_);

  // resolve dot-dot elements
  fspath = std::filesystem::weakly_canonical(fspath, ec);
  if (ec) {
    DLOG_F("failed std::filesystem::canonical(%s).", raw_path_.c_str());
    return false;
  }

  // convert to absolute
  if (!fspath.is_absolute()) {
    fspath = std::filesystem::absolute(fspath, ec);
    if (ec) {
      DLOG_F("failed std::filesystem::absolute(%s).", raw_path_.c_str());
      return false;
    }
  }

  // convert all separators
  fspath = fspath.make_preferred();

  name_ = str::from_u8string(fspath.filename().u8string());
  path_ = str::from_u8string(fspath.u8string());
  valid_ = true;

  // directory
  std::string directory;
  if (std::filesystem::is_directory(fspath)) {
    directory = str::from_u8string(fspath.u8string());
  } else {
    directory = str::from_u8string(fspath.parent_path().u8string());
  }

  if (reference.valid() && reference.cd() == directory) {
    children_ = std::move(reference.children_);
    sort_type_ = reference.sort_type_;
    sort_desc_ = reference.sort_desc_;
  } else {
    children_ = fetch_children(directory, true, false);
  }

  ordinal_ = 0;
  for (int i = 0; i < (int)children_.size(); ++i) {
    if (children_[i].name == name_) {
      ordinal_ = i;
    }
  }

  return true;
}

std::string FileInfo::raw_path() const { return raw_path_; }

bool FileInfo::valid() const { return valid_; }

const std::string& FileInfo::path() const { return path_; }

const std::string& FileInfo::name() const { return name_; }

std::string FileInfo::cd() const { 
  if (flags_ & Directory) {
    return path_;
  } else {
    std::filesystem::path fspath(path_);
    return str::from_u8string(fspath.parent_path().u8string());
  }
}

int FileInfo::ordinal() const { return ordinal_; }

size_t FileInfo::size() const { return size_; }

std::string FileInfo::parent() const {
  if (flags_ & Directory) {
    std::filesystem::path path(path_);
    std::filesystem::path parent_path(path.parent_path());
    return str::from_u8string(parent_path.u8string());
  } else {
    return cd();
  }
}

std::chrono::system_clock::time_point FileInfo::created() const {
  return std::chrono::system_clock::time_point();
}

std::chrono::system_clock::time_point FileInfo::modified() const {
  return std::chrono::system_clock::time_point();
}

int FileInfo::flags() const { return flags_; }

std::vector<DirEntry> FileInfo::fetch_children(const std::string& directory,
    bool include_file, bool include_directory) const { 
  std::vector<DirEntry> children;

  std::string pattern = directory + kNativeSeparator + "*.*";

  // Windows
  WIN32_FIND_DATA data{};
  HANDLE handle = ::FindFirstFile(pattern.c_str(), &data);
  if (handle == INVALID_HANDLE_VALUE) {
    DLOG_F("failed to ::FindFirstFile().");
    return children;
  }
  do {
    int flags = None;
    size_t size = 0;
    if (::strcmp(data.cFileName, ".") == 0 ||
        ::strcmp(data.cFileName, "..") == 0) {
      continue;
    } else if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      if (!include_directory) {
        continue;
      }
      flags |= Directory;
    } else {
      if (!include_file) {
        continue;
      }
      if (data.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) {
        flags |= System;
      } else if (data.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) {
        flags |= Hidden;
      } else {
        ULARGE_INTEGER ul;
        ul.HighPart = data.nFileSizeHigh;
        ul.LowPart = data.nFileSizeLow;
        size = ul.QuadPart;
      }
    }

    DirEntry de;
    de.name = data.cFileName;
    de.path = directory + kNativeSeparator + data.cFileName;
    de.size = size;
    de.created = filetime_to_system_clock(&data.ftCreationTime);
    de.modified = filetime_to_system_clock(&data.ftLastWriteTime);
    de.flags = flags;
    children.emplace_back(std::move(de));
  } while (::FindNextFile(handle, &data) != 0);

  return children;
}

const std::vector<DirEntry>& FileInfo::children() const {
  return children_;
}

bool FileInfo::sort(SortType sort_type, bool sort_desc) {
  std::function<bool(const DirEntry&, const DirEntry&, bool)> comparer;
  if (sort_type == SortType::Name) {
    comparer = comparer::name;
  } else if (sort_type == SortType::Size) {
    comparer = comparer::size;
  } else if (sort_type == SortType::Created) {
    comparer = comparer::created;
  } else if (sort_type == SortType::Modified) {
    comparer = comparer::modified;
  } else {
    return false;
  }

  std::stable_sort(children_.begin(), children_.end(),
      std::bind(comparer, std::placeholders::_1, std::placeholders::_2, sort_desc));

  for (int i = 0; i < (int)children_.size(); ++i) {
    children_[i].ordinal = i;
    if (children_[i].name == name_) {
      ordinal_ = i;
    }
  }

  sort_type_ = sort_type;
  sort_desc_ = sort_desc;
  return true;
}

FileStream::FileStream(const std::string& path) : handle_(), path_() {
  DWORD desired_access = GENERIC_READ;
  DWORD share_mode = FILE_SHARE_READ;
  DWORD flags = FILE_ATTRIBUTE_NORMAL;
  handle_ = ::CreateFile(path.c_str(), desired_access, share_mode, NULL,
      OPEN_EXISTING, flags, NULL);
  if (handle_ == INVALID_HANDLE_VALUE) {
    throw std::runtime_error("failed CreateFile().");
  }
  path_ = std::filesystem::path(path);
}

FileStream::~FileStream() {
  if (handle_ != NULL) {
    ::CloseHandle(handle_);
  }
}

size_t FileStream::Read(uint8_t* dst, size_t size) {
  DWORD read_bytes{};
  BOOL ret = ::ReadFile(handle_, dst, (DWORD)size, &read_bytes, NULL);
  if (ret == FALSE) {
    DWORD error = ::GetLastError();
    throw std::runtime_error(error_message(error));
  }

  return read_bytes;
}

size_t FileStream::Seek(size_t pos) {
  LARGE_INTEGER dist, after;
  dist.QuadPart = pos;
  BOOL ret = ::SetFilePointerEx(handle_, dist, &after, FILE_BEGIN);
  if (ret == FALSE) {
    DWORD error = ::GetLastError();
    throw std::runtime_error(error_message(error));
  }
  return after.QuadPart;
}

size_t FileStream::Size() {
  LARGE_INTEGER size;
  BOOL ret = ::GetFileSizeEx(handle_, &size);
  if (ret == FALSE) {
    DWORD error = ::GetLastError();
    throw std::runtime_error(error_message(error));
  }
  return size.QuadPart;
}

FileReader::FileReader(const std::string& path, size_t prefetch_size)
    : pos_(0) {
  filestream_ = std::unique_ptr<FileStream>(new FileStream(path));
  size_ = filestream_->Size();
  pos_ = -1;
  prefetched_.resize(prefetch_size, 0);

  // Prefetch
  Read(0, prefetch_size);
}

FileReader::~FileReader() {}

const uint8_t* FileReader::Read(size_t pos, size_t size) {
  if (pos > size_) {
    return nullptr;
  }

  if (pos < pos_ || pos >= (pos_ + prefetched_.size())) {
    size_t cache_size = prefetched_.size();
    size_t aligned = pos / cache_size * cache_size;
    if (pos_ != aligned) {
      size_t after = filestream_->Seek(aligned);
      if (after != aligned) {
        throw std::runtime_error("failed Seek().");
      }
    }
    pos_ = aligned;

    size_t avail = size_ - pos;
    size_t read_bytes = filestream_->Read(prefetched_.data(), std::min(avail, cache_size));
  }

  return prefetched_.data() + (pos - pos_);
}

}  // namespace chaos
