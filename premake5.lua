name = "chaos"

workspace (name)
    configurations { "Debug", "Release" }
    platforms { "x64" }
    cppdialect "C++20"
    location "build"

project (name)
    kind "StaticLib"
    language "C++"
    characterset "ASCII"
    toolset "v143"
    configurations { "Debug", "Release" }
    platforms { "x64" }
    flags { "MultiProcessorCompile", "NoMinimalRebuild" } -- /MP

    location "build"
    objdir "build/obj/%{cfg.platform}/%{cfg.buildcfg}"
    targetdir "build/bin/%{cfg.platform}/%{cfg.buildcfg}"

    files {
        "**.cc",
        "**.cpp",
        "**.h",
    }
    includedirs { 
        "./",
        "graphics/impl/directxheaders/include",
        "graphics/impl/directxheaders/include/directx",
        "graphics/impl/directxheaders/include/dxguids",
        "graphics/impl/directxheaders/include/wsl",
        "graphics/impl/d3d12memoryallocator",
        "extras"
    }
    removefiles { "examples/**" }

    pchsource "pch.cc"
    pchheader "pch.h"
    forceincludes { "pch.h" }

    filter { "platforms:x64" }
        system "Windows"
        architecture "x86_64"
        buildoptions { "/execution-charset:utf-8" }
    filter { "configurations:Debug" }
        defines { "_DEBUG" }
        optimize "Debug"
        symbols "On"
    filter { "configurations:Release" }
        defines { "NDEBUG" }
        optimize "Speed"
    filter { "files:**.hlsl" }
        local shader_variable_name = "%{file.basename:gsub('%.', '_')}"
        local shader_header_path = "%{file.directory}/compiled/%{file.basename}.h"
        buildmessage "Compiling HLSL with dxc (%{file.relpath}) ..."
        buildoutputs { shader_header_path }
    -- /Zpr: Packs matrices in row-major order by default
    filter { "files:**.vs.hlsl" }
        buildcommands { "dxc -T vs_6_5 %{file.relpath} -Zpr -Fh " .. shader_header_path .. " -Vn " .. shader_variable_name }
    filter { "files:**.fs.hlsl" }
        buildcommands { "dxc -T ps_6_5 %{file.relpath} -Zpr -Fh " .. shader_header_path .. " -Vn " .. shader_variable_name }

project (name .. ".examples.helloworld")
    dependson {name}

    kind "ConsoleApp"
    files { "examples/helloworld/*.*" }
    links { "build/bin/%{cfg.platform}/%{cfg.buildcfg}/" .. name .. ".lib" }
    includedirs { "./" }

    location "build"
    objdir "build/obj/%{cfg.platform}/%{cfg.buildcfg}"
    targetdir "build/bin/%{cfg.platform}/%{cfg.buildcfg}"

    filter { "platforms:x64" }
        system "Windows"
        architecture "x86_64"
        buildoptions { "/execution-charset:utf-8" }
    filter { "configurations:Debug" }
        defines { "_DEBUG" }
        optimize "Debug"
        symbols "On"
    filter { "configurations:Release" }
        defines { "NDEBUG" }
        optimize "Speed"
