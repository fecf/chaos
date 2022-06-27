#pragma once

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace chaos {

std::string getCurrentDirectory();
std::string getUserDirectory();
std::string getFontDirectory();
std::vector<std::string> getCommandLineArgs();

enum class SortType {
  Unsorted,
  Name,
  Size,
  Created,
  Modified,
};

extern const std::string kNativeSeparator;

struct DirEntry {
  std::string name;
  std::string path;
  size_t size;
  std::chrono::system_clock::time_point created;
  std::chrono::system_clock::time_point modified;
  int flags;
  int ordinal;
};

class Handle {
 public:
  enum Flags {
    None = 0x00,
    Directory = 0x01,
    Hidden = 0x02,
    System = 0x04,
  };
  using Time = std::chrono::system_clock::time_point;

  Handle();
  explicit Handle(const std::string& path);
  Handle& operator=(const std::string& path);

  std::string raw_path() const;
  bool valid() const;
  const std::string& path() const;  // utf-8
  const std::string& name() const;  // utf-8
  std::string cd() const;
  int ordinal() const;
  size_t size() const;
  std::string parent() const;
  Time created() const;
  Time modified() const;
  int flags() const;

  const std::vector<DirEntry>& children() const;
  bool sort(SortType sort_type = SortType::Name, bool sort_desc = false);
  bool is_sorted(SortType type, bool desc) const;

 private:
  std::vector<DirEntry> fetch_children(const std::string& fullpath,
      bool include_file, bool include_directory) const;
  bool refresh(bool force_refresh_children = false);

 private:
  std::string raw_path_;
  bool valid_;
  std::string path_;
  std::string name_;
  int ordinal_;
  size_t size_;
  int flags_;
  Time created_;
  Time modified_;

  std::vector<DirEntry> children_;
  SortType sort_type_;
  bool sort_desc_;
};

class FileStream {
 public:
  FileStream(const std::string& path);
  ~FileStream();

  FileStream(const FileStream&) = delete;
  FileStream& operator=(const FileStream&) = delete;

  size_t Read(uint8_t* dst, size_t size);
  size_t Seek(size_t pos);
  size_t Size();

  std::filesystem::path path() const { return path_; }

 private:
  void* handle_;
  std::filesystem::path path_;
};

class FileReader {
 public:
  FileReader(const std::string& path, size_t prefetch_size = 1024 * 1024);
  ~FileReader();

  const uint8_t* Read(size_t pos, size_t size);
  size_t GetSize() const { return size_; }

 private:
  size_t pos_;
  size_t size_;
  std::vector<uint8_t> prefetched_;
  std::unique_ptr<FileStream> filestream_;
};

}  // namespace chaos

