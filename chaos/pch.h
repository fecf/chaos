#pragma once

// STL Headers
#include <algorithm>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <future>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <unordered_map>
#include <vector>

#include "base/minlog.h"

#include "graphics/impl/d3d12def.h"
#include "graphics/imgui/imconfig.h"
#include "graphics/imgui/imgui.h"

#include "extras/json.hpp"
#include "extras/gltf.h"
#include "extras/linalg.h"
#include "extras/simpledwrite.h"

#include "base/win32def.h"
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <wrl/client.h>
