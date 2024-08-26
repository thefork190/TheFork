cmake_minimum_required (VERSION 3.15)

set(namespace "TF")
project("RHI"
	VERSION 0.1.0
    DESCRIPTION "The-Forge RHI"
)

if (NOT CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    message(STATUS "This project has a top-level one called [${CMAKE_PROJECT_NAME}]")
else()
    message(STATUS "This project is a top-level one")
endif()

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(${PROJECT_NAME} STATIC)

target_include_directories(${PROJECT_NAME}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Private
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Public>

)

# totally optional listing of include directories
get_property(inclds
    TARGET ${PROJECT_NAME}
    PROPERTY INCLUDE_DIRECTORIES
)
message(STATUS "Listing include directories for ${PROJECT_NAME}...")
# "dumb" listing with "raw" generator expressions on configuration
foreach(pth ${inclds})
    message(STATUS "  ${pth}")
endforeach()
# actually evaluated generator expressions printed to file on generation
string(JOIN "\n" includeDirectories ${inclds})
file(GENERATE
    OUTPUT "${CMAKE_BINARY_DIR}/include-directories.txt"
    CONTENT ${includeDirectories}
)

# without it public headers won't get installed
set(public_headers
    Public/Config.h
	Public/IFileSystem.h
	Public/IGraphics.h
	Public/ILog.h
	Public/IMemory.h
	Public/IOperatingSystem.h
	Public/IRay.h
	Public/IResourceLoader.h
	Public/IShaderReflection.h
	Public/IThread.h
	Public/ITime.h
	Public/IToolFileSystem.h
	Public/RingBuffer.h
)


# Spirv Tools
set(SPIRV_SOURCES 
	"../ThirdParty/SPIRV_Cross/spirv_cfg.cpp"
	"../ThirdParty/SPIRV_Cross/spirv_cpp.cpp"
	"../ThirdParty/SPIRV_Cross/spirv_cross.cpp"
	"../ThirdParty/SPIRV_Cross/spirv_cross_Parsed_ir.cpp"
	"../ThirdParty/SPIRV_Cross/spirv_cross_util.cpp"
	"../ThirdParty/SPIRV_Cross/spirv_glsl.cpp"
	"../ThirdParty/SPIRV_Cross/spirv_hlsl.cpp"
	"../ThirdParty/SPIRV_Cross/spirv_msl.cpp"
	"../ThirdParty/SPIRV_Cross/spirv_parser.cpp"
	"../ThirdParty/SPIRV_Cross/spirv_reflect.cpp"
	"../ThirdParty/SPIRV_Cross/SpirvTools.cpp"
)

# BString
set(BSTRLIB_SOURCES
	"../ThirdParty/bstrlib/bstrlib.c"
	"Private/Math/StbDs.c"
)

# RHI (min. requirement for rendering)
set(RHI_SOURCES 
	"Private/Graphics/CommonShaderReflection.cpp"
	"Private/Graphics/GraphicsConfig.cpp"
	"Private/Graphics/PickRenderingApi.cpp"

	"Private/ResourceLoader/ResourceLoader.cpp"

	"Private/ResourceLoader/ResourceLoader.cpp"
	
	"Private/Graphics/Direct3D12/Direct3D12Hooks.cpp"
	"Private/Graphics/Direct3D12/Direct3D12.cpp"
	"Private/Graphics/Direct3D12/Direct3D12Raytracing.cpp"
	"Private/Graphics/Direct3D12/Direct3D12ShaderReflection.cpp"
	
	"Private/Graphics/Vulkan/Vulkan.cpp"
	"Private/Graphics/Vulkan/VulkanRaytracing.cpp"
	"Private/Graphics/Vulkan/VulkanShaderReflection.cpp"
)

# Core functionalities
set(CORE_SOURCES 
	"Private/Log/Log.c"
	"Private/MemoryTracking/MemoryTracking.c"
	"Private/FileSystem/FileSystem.c"

	"Private/OS/Timer.c"

	"Private/OS/Windows/WindowsThread.c"
	"Private/OS/Windows/WindowsLog.c"
	"Private/OS/Windows/WindowsFileSystem.cpp"
	"Private/OS/Windows/WindowsTime.c"
)

target_sources(${PROJECT_NAME}
	PRIVATE
		${RENDERING_SOURCES}
		${SPIRV_SOURCES}
		${RHI_SOURCES}
		${CORE_SOURCES}
		${BSTRLIB_SOURCES}
)

target_link_libraries(${PROJECT_NAME}
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/DirectXShaderCompiler/lib/x64/dxcompiler.lib
		${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/nvapi/amd64/nvapi64.lib
		${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/ags/ags_lib/lib/amd_ags_x64.lib
		${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/winpixeventruntime/bin/WinPixEventRuntime.lib
)

# where to find our CMake modules
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
#message(STATUS "CMake module path: ${CMAKE_MODULE_PATH}")
include(Installing)

# generate and install export file
install(EXPORT "${PROJECT_NAME}Targets"
    FILE "${PROJECT_NAME}Targets.cmake"
    NAMESPACE ${namespace}::
    DESTINATION cmake
)

include(CMakePackageConfigHelpers)

# generate the version file for the config file
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
    VERSION "${version}"
    COMPATIBILITY AnyNewerVersion
)
# create config file
configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/Config.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
    INSTALL_DESTINATION cmake
)
# install config files
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
    DESTINATION cmake
)