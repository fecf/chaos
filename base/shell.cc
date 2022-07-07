#include "shell.h"
#include "base/text.h"
#include "image/image.h"

#include <stdexcept>

#include <wrl.h>
using namespace Microsoft::WRL;

#include <ShObjIdl_core.h>


namespace chaos {

std::string OpenDialog(void* parent, const std::string& name, bool folder) {
  ScopedCoInitialize coinit;

  ComPtr<IFileOpenDialog> dialog;
  HRESULT hr;
  hr = ::CoCreateInstance(
      CLSID_FileOpenDialog, 0, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
  if (FAILED(hr)) {
    throw std::runtime_error("CoCreateInstance().");
  }

  PWSTR file_path{};
  ComPtr<IShellItem> com_shell_item_;

  hr = dialog->SetTitle(str::widen(name).c_str());
  if (FAILED(hr)) {
    throw std::runtime_error("IFileOpenDialog::SetTitle().");
  }

  if (folder) {
    hr = dialog->SetOptions(FOS_PICKFOLDERS);
    if (FAILED(hr)) {
      throw std::runtime_error("IFileOpenDialog::SetOptions().");
    }
  }

  hr = dialog->Show((HWND)parent);
  if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
    return {};
  } else if (FAILED(hr)) {
    throw std::runtime_error("failed IFileOpenDialog::Show().");
  }

  hr = dialog->GetResult(&com_shell_item_);
  if (FAILED(hr)) {
    throw std::runtime_error("failed IFileOpenDialog::GetResult().");
  }

  hr = com_shell_item_->GetDisplayName(SIGDN_FILESYSPATH, &file_path);
  if (FAILED(hr)) {
    throw std::runtime_error("failed IShellItem::GetDisplayName().");
  }

  std::string ret = str::narrow(file_path);
  return ret;
}

std::string SaveDialog(
    void* parent, const std::string& name, const std::string& extension) {
  ScopedCoInitialize coinit;

  ComPtr<IFileSaveDialog> dialog;

  HRESULT hr = ::CoCreateInstance(
      CLSID_FileSaveDialog, 0, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
  if (FAILED(hr)) {
    throw std::runtime_error("CoCreateInstance().");
  }

  PWSTR file_path{};
  ComPtr<IShellItem> com_shell_item_;

  hr = dialog->SetTitle(str::widen(name).c_str());
  if (FAILED(hr)) {
    throw std::runtime_error("IFileSaveDialog::SetTitle().");
  }

  hr = dialog->Show((HWND)parent);
  if (FAILED(hr)) {
    throw std::runtime_error("failed IFileSaveDialog::Show().");
  }

  hr = dialog->GetResult(&com_shell_item_);
  if (FAILED(hr) || !com_shell_item_) {
    throw std::runtime_error("failed IFileSaveDialog::GetResult().");
  }

  hr = com_shell_item_->GetDisplayName(SIGDN_FILESYSPATH, &file_path);
  if (FAILED(hr)) {
    throw std::runtime_error("failed IShellItem::GetDisplayName().");
  }

  std::string ret = str::narrow(file_path);
  return ret;
}

bool OpenFolder(const std::string& dir) {
  std::filesystem::path path(dir);
  std::error_code ec;
  if (!std::filesystem::exists(path, ec) || ec) {
    return false;
  }

  std::string str = path.string();
  if (!std::filesystem::is_directory(path)) {
    str = path.parent_path().string();
  }

  HINSTANCE ret =
      ::ShellExecute(NULL, "open", str.c_str(), NULL, NULL, SW_SHOWNORMAL);
  return static_cast<int>(reinterpret_cast<uintptr_t>(ret)) >= 32;
}

void OpenURL(const std::string& url) {
  ::ShellExecuteA(0, 0, url.c_str(), 0, 0, SW_SHOW);
}

void OpenControlPanelAppsDefaults() {
  ComPtr<IApplicationActivationManager> activator;
  HRESULT hr = CoCreateInstance(CLSID_ApplicationActivationManager, nullptr,
      CLSCTX_INPROC, IID_IApplicationActivationManager, (void**)&activator);

  if (SUCCEEDED(hr)) {
    DWORD pid;
    hr = activator->ActivateApplication(
        L"windows.immersivecontrolpanel_cw5n1h2txyewy"
        L"!microsoft.windows.immersivecontrolpanel",
        L"page=SettingsPageAppsDefaults", AO_NONE, &pid);
    if (SUCCEEDED(hr)) {
      // Do not check error because we could at least open
      // the "Default apps" setting.
      activator->ActivateApplication(
          L"windows.immersivecontrolpanel_cw5n1h2txyewy"
          L"!microsoft.windows.immersivecontrolpanel",
          L"page=SettingsPageAppsDefaults"
          L"&target=SystemSettings_DefaultApps_Browser",
          AO_NONE, &pid);
    }
  }
}

std::string GetCurrentProcessPath() { 
  char buf[1024];
  ::GetModuleFileName(NULL, buf, 1024);
  return buf;
}

bool SetClipboardImage(chaos::Image* image) {
  if (!::OpenClipboard(NULL)) {
    return false;
  }
  if (!::EmptyClipboard()) {
    return false;
  }
  std::vector<uint8_t> png = Image::Save(image, "png");
  HGLOBAL hglobal = ::GlobalAlloc(GMEM_MOVEABLE, png.size());
  if (hglobal == NULL) {
    return false;
  }
  LPVOID ptr = ::GlobalLock(hglobal);
  if (ptr == NULL) {
    return false;
  }
  ::memcpy(ptr, png.data(), png.size());
  if (::GlobalUnlock(hglobal) != 0 || ::GetLastError() != NO_ERROR) {
    // returns zero = unlocked
    return false;
  }
  static UINT format = ::RegisterClipboardFormat("PNG");
  HANDLE handle = ::SetClipboardData(format, hglobal);
  if (!handle) {
    return false;
  }
  ::CloseClipboard();
  return true;
}

bool SetClipboardText(const std::string& text) {
  if (!::OpenClipboard(NULL)) {
    return false;
  }
  if (!::EmptyClipboard()) {
    return false;
  }
  HGLOBAL hglobal = ::GlobalAlloc(GMEM_MOVEABLE, text.size());
  if (hglobal == NULL) {
    return false;
  }
  LPVOID ptr = ::GlobalLock(hglobal);
  if (ptr == NULL) {
    return false;
  }
  ::memcpy(ptr, text.data(), text.size());
  if (!::GlobalUnlock(hglobal)) {
    return false;
  }
  HANDLE handle = ::SetClipboardData(CF_TEXT, hglobal);
  if (!handle) {
    return false;
  }
  ::CloseClipboard();
  return true;
}

std::vector<std::pair<std::string, std::string>> GetLogicalDrives() {
  std::vector<std::pair<std::string, std::string>> ret;

  char drives[MAX_PATH]{};
  int len = ::GetLogicalDriveStrings(MAX_PATH, drives);
  if (len < 0) {
    return {};
  }

  for (int i = 0, begin = 0, end = 0; i < len; ++i) {
    if (drives[i] == '\0') {
      end = i;
      std::string letter(drives + begin, drives + end);
      begin = i + 1;

      char volname[256], fsname[256];
      DWORD serial, component, flags;
      BOOL success = ::GetVolumeInformation(letter.c_str(), volname, 256,
          &serial, &component, &flags, fsname, 256);
      if (success == FALSE) {
        continue;
      }

      ret.push_back({letter, volname});
    }
  }

  return ret;
}

}  // namespace chaos
