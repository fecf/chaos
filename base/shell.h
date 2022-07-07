#pragma once

#include <string>
#include <vector>
#include <utility>

namespace chaos {

class Image;

// Common Dialog
std::string OpenDialog(void* parent, const std::string& name, bool folder);
std::string SaveDialog(
    void* parent, const std::string& name, const std::string& extension);

// Drive
std::vector<std::pair<std::string, std::string>> GetLogicalDrives();

// Shell
bool OpenFolder(const std::string& dir);
void OpenURL(const std::string& url);
void OpenControlPanelAppsDefaults();
std::string GetCurrentProcessPath();

// Clipboard
bool SetClipboardImage(chaos::Image* image);
bool SetClipboardText(const std::string& text);

}  // namespace chaos
