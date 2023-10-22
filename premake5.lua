-- main solution file.
workspace "VulkanPlayground"
    architecture "x64"

    configurations 
    { 
        "Debug",
        "Release" 
    }

    startproject "VulkanApp"

-- Variable to hold output directory.
outputdir = "%{cfg.buildcfg}-%{cfg.architecture}"

-- Engine project.
project "VulkanEngine"
    location "VulkanEngine"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "on"

    -- Directories for binary and intermediate files.
    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

    files 
    { 
        "%{prj.name}/src/**.h", 
        "%{prj.name}/src/**.cpp"
    }

    defines
	{
		"_CRT_SECURE_NO_WARNINGS",
		"GLFW_INCLUDE_VULKAN"
	}

    -- Include directories.
    includedirs
    {
        "C:/VulkanSDK/1.3.239.0/Include",
        "Dependencies/glfw/include",
        "Dependencies/glm",
        "Dependencies"
    }

    libdirs
    {
        "C:/VulkanSDK/1.3.239.0/Lib",
        "Dependencies/glfw/lib-vc2019/x64"
    }

    links
    {
        "glfw3.lib",
        "vulkan-1.lib"
    }

    filter "system:windows"
        systemversion "latest"

    filter "configurations:Debug"
        defines { "DEBUG" }
        symbols "On"

    filter "configurations:Release"
        defines { "RELEASE" }
        optimize "On"
        
    filter { "system:windows", "configurations:Debug" }
        buildoptions "/MDd"

    filter { "system:windows", "configurations:Release" }
        buildoptions "/MD"

-- App project.
project "VulkanApp"
    location "VulkanApp"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"
    staticruntime "on"

    -- Directories for binary and intermediate files.
    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

    files 
    { 
        "%{prj.name}/src/**.h", 
        "%{prj.name}/src/**.cpp"
    }

    defines
	{
		"_CRT_SECURE_NO_WARNINGS",
		"GLFW_INCLUDE_VULKAN"
	}

    -- Include directories.
    includedirs
    {
        "C:/VulkanSDK/1.3.239.0/Include",
        "VulkanEngine/src",
        "Dependencies/glfw/include",
        "Dependencies/glm",
        "Dependencies"
    }

    links
    {
        "VulkanEngine"
    }

    filter "system:windows"
        systemversion "latest"

    filter "configurations:Debug"
        defines { "DEBUG" }
        symbols "On"

    filter "configurations:Release"
        defines { "RELEASE" }
        optimize "On"

    filter { "system:windows", "configurations:Debug" }
        buildoptions "/MDd"

    filter { "system:windows", "configurations:Release" }
        buildoptions "/MD"