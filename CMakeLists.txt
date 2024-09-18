cmake_minimum_required(VERSION 3.16)

# set the output directory for built objects.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")

# prevent installing to system directories.
set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}" CACHE INTERNAL "")

# Declare the project
project("TheFork"
    VERSION 0.1.0
    DESCRIPTION "An ECS engine."
)

if(APPLE)
    enable_language(OBJC)
    enable_language(OBJCXX)
endif()
# Set the name of the executable
set(EXECUTABLE_NAME ${PROJECT_NAME})

# Collect all shader files recursively from the FSL directory
file(GLOB_RECURSE SHADER_FILES "${CMAKE_SOURCE_DIR}/Source/FSL/*")

# Create a custom target for the shaders (they won't be compiled)
add_custom_target(ShaderFiles SOURCES ${SHADER_FILES})

# Group shaders under "FSL Shaders" in Visual Studio, preserving folder structure
foreach(_shader IN ITEMS ${SHADER_FILES})
    # Get the path relative to the Source/FSL directory
    file(RELATIVE_PATH _relPath "${CMAKE_SOURCE_DIR}/Source/FSL" "${_shader}")
    
    # Get the directory part of the relative path (strip the filename)
    get_filename_component(_groupPath "${_relPath}" DIRECTORY)
    
    # Replace slashes with backslashes for Visual Studio filters
    string(REPLACE "/" "\\" _groupPath "${_groupPath}")
    
    # If the file is in a subdirectory, group it properly
    if(_groupPath)
        source_group("FSL\\${_groupPath}" FILES "${_shader}")
    else()
        # If the file is directly in Source/FSL, put it in the "FSL" group
        source_group("FSL" FILES "${_shader}")
    endif()
endforeach()

# Function to automatically create source groups based on file paths
function(create_source_groups)
    foreach(_source IN ITEMS ${ARGN})
        get_filename_component(_source_path "${_source}" PATH)
        string(REPLACE "${CMAKE_SOURCE_DIR}/Source" "Source" _group_path "${_source_path}")
        string(REPLACE "/" "\\" _group_path "${_group_path}")
        source_group("${_group_path}" FILES "${_source}")
    endforeach()
endfunction()

set(IOSASSETS 
    Source/iosLaunchScreen.storyboard
    Source/Sample.appxManifest
)

# Imgui sources
set(IMGUI_SRC 
    Source/Modules/Medium/Imgui/imconfig.h
    ThirdParty/imgui/imgui.cpp
    ThirdParty/imgui/imgui.h
    ThirdParty/imgui/imgui_demo.cpp
    ThirdParty/imgui/imgui_draw.cpp
    ThirdParty/imgui/imgui_internal.h
    ThirdParty/imgui/imgui_tables.cpp
    ThirdParty/imgui/imgui_widgets.cpp
    ThirdParty/imgui/imstb_rectpack.h
    ThirdParty/imgui/imstb_textedit.h
    ThirdParty/imgui/imstb_truetype.h
    # we use our own version of the vanilla SDL3 backend before of how often SDL3 still changes
    Source/Modules/Medium/Imgui/imgui_impl_sdl3.h
    Source/Modules/Medium/Imgui/imgui_impl_sdl3.cpp
    )

if (WIN32)
set(IMGUI_RENDERING_IMPL
    ThirdParty/imgui/backends/imgui_impl_vulkan.h
    ThirdParty/imgui/backends//imgui_impl_vulkan.cpp
    )
endif (WIN32)

# List all sources
set(SOURCES
    ${IMGUI_SRC}
    ${IMGUI_RENDERING_IMPL}

    # Engine
    Source/Main.cpp
    Source/Modules/LifeCycledModule.h
    # LOW
    Source/Modules/Low/Engine.h
    Source/Modules/Low/Engine.cpp
    Source/Modules/Low/Inputs.h
    Source/Modules/Low/Inputs.cpp
    Source/Modules/Low/RHI.h
    Source/Modules/Low/RHI.cpp
    Source/Modules/Low/Window.h
    Source/Modules/Low/Window.cpp
    # MEDIUM
    Source/Modules/Medium/FontRendering.h
    Source/Modules/Medium/FontRendering.cpp
    # High
    Source/Modules/High/FlappyClone/FlappyClone.h
    Source/Modules/High/FlappyClone/FlappyClone.cpp
    Source/Modules/High/HelloTriangle/HelloTriangle.h
    Source/Modules/High/HelloTriangle/HelloTriangle.cpp
)

if(APPLE)
# could be more granular than that.
# we shouldn't have to compile everything as objc++
set_source_files_properties(
    ${SOURCES} PROPERTIES LANGUAGE OBJCXX
)
endif()
# Call the function to create source groups based on the source files
create_source_groups(${SOURCES} ${IOSASSETS})

# Create an executable or shared library based on the platform and add our sources to it
if (ANDROID)
    # The SDL java code is hardcoded to load libmain.so on android, so we need to change EXECUTABLE_NAME
    set(EXECUTABLE_NAME main)
    add_library(${EXECUTABLE_NAME} SHARED)
else()
    add_executable(${EXECUTABLE_NAME})
endif()

# Add sources to target
target_sources(${EXECUTABLE_NAME}
    PRIVATE ${SOURCES} ${IOSASSETS}
)

# Add additional platform-specific settings
if (WINDOWS_STORE)
    target_sources("${EXECUTABLE_NAME}" PRIVATE 
        Source/logo44.png 
        Source/logo150.png 
        Source/logo50.png 
        Source/logo620x300.png
    )
elseif(APPLE)
    target_sources("${EXECUTABLE_NAME}" PRIVATE Source/logo.png)
    set(CMAKE_EXE_LINKER_FLAGS "-framework Cocoa -framework AppKit -framework CoreData -framework Foundation -framework Metal -framework MetalPerformanceShaders -framework CoreVideo -framework IOKit")
endif()

# use C++20
target_compile_features(${EXECUTABLE_NAME} PUBLIC cxx_std_20)

if(CMAKE_SYSTEM_NAME MATCHES Emscripten)
    set(CMAKE_EXECUTABLE_SUFFIX ".html" CACHE INTERNAL "")
endif()

if(WINDOWS_STORE)
    target_compile_options(${EXECUTABLE_NAME} PUBLIC "/EHsc")
endif()

# 3rd Parties
add_subdirectory(ThirdParty/SDL EXCLUDE_FROM_ALL)
add_subdirectory(ThirdParty/The-Forge/RHI EXCLUDE_FROM_ALL)
add_subdirectory(ThirdParty/flecs EXCLUDE_FROM_ALL)
add_subdirectory(ThirdParty/glm EXCLUDE_FROM_ALL)

target_include_directories(${PROJECT_NAME}
    PRIVATE "Source/Modules"
    PRIVATE ThirdParty/The-Forge/RHI/Private    # Hack for TF
    PRIVATE ThirdParty/The-Forge/ThirdParty     # Hack for TF
    PUBLIC ThirdParty/The-Forge/RHI/Public
    PRIVATE ThirdParty/CLI11/include/CLI                        # CLI11 header only 3rd party
    PRIVATE ThirdParty/imgui                                    # Required to compile imgui
    PRIVATE ThirdParty/The-Forge/ThirdParty/VulkanSDK/include   # For imgui VK backend
)
if(APPLE)
target_link_libraries(${EXECUTABLE_NAME} 
    PRIVATE
        RHI
        flecs_static
        glm::glm
        $<$<CONFIG:Debug>:${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/efsw_artifacts/mac-lib/libefsw-static-debug.a>
        $<$<CONFIG:Release>:${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/efsw_artifacts/mac-lib/libefsw-static-release.lib>
        $<$<CONFIG:RelWithDebInfo>:${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/efsw_artifacts/mac-lib/libefsw-static-reldbginfo.lib>
    PUBLIC 
        SDL3::SDL3
)
else()
target_link_libraries(${EXECUTABLE_NAME} 
    PRIVATE
        RHI
        flecs_static
        glm::glm
        $<$<CONFIG:Debug>:${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/efsw_artifacts/lib/efsw-static-debug.lib>
        $<$<CONFIG:Release>:${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/efsw_artifacts/lib/efsw-static-release.lib>
        $<$<CONFIG:RelWithDebInfo>:${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/efsw_artifacts/lib/efsw-static-reldbginfo.lib>
    PUBLIC 
        SDL3::SDL3
)
endif()
target_compile_definitions(${EXECUTABLE_NAME} 
    PUBLIC SDL_MAIN_USE_CALLBACKS
    PUBLIC IMGUI_USER_CONFIG="Medium/Imgui/imconfig.h"
    )

set_target_properties(${EXECUTABLE_NAME} PROPERTIES 
    MACOSX_BUNDLE TRUE
    XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES  
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Source/Info.plist.in"
    XCODE_GENERATE_SCHEME TRUE
    XCODE_ATTRIBUTE_BUNDLE_IDENTIFIER "com.sharmoota.the-fork"
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.sharmoota.the-fork"
    XCODE_ATTRIBUTE_CURRENTYEAR "${CURRENTYEAR}"
)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT "${EXECUTABLE_NAME}")

if(CMAKE_SYSTEM_NAME MATCHES "Darwin")
    INSTALL(CODE 
        "include(BundleUtilities)
        fixup_bundle(\"$<TARGET_BUNDLE_DIR:${EXECUTABLE_NAME}>\" \"\" \"\")
        " 
        COMPONENT Runtime
    )
endif()

install(TARGETS ${EXECUTABLE_NAME} BUNDLE DESTINATION ./install)

if (WIN32)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/The-Forge/ThirdParty/winpixeventruntime/bin/WinPixEventRuntime.dll
        ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/The-Forge/ThirdParty/DirectXShaderCompiler/bin/x64/dxcompiler.dll
        ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/The-Forge/ThirdParty/ags/ags_lib/lib/amd_ags_x64.dll
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
    )
endif (WIN32)

# Assets
set(ASSETS_DIR "${CMAKE_SOURCE_DIR}/Assets")
file(GLOB_RECURSE ASSET_FILES "${ASSETS_DIR}/*")
if(APPLE)
# Define the path to the Resources folder inside the .app bundle
set(ASSETS_DESTINATION "$<TARGET_BUNDLE_DIR:${EXECUTABLE_NAME}>/Contents/Resources/Assets")

# Define a post-build command to copy assets to the .app bundle
add_custom_command(
    TARGET ${EXECUTABLE_NAME}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${ASSETS_DESTINATION}"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${ASSETS_DIR}" "${ASSETS_DESTINATION}"
    COMMENT "Copying assets to .app bundle..."
)
else()
# Define the path to the Resources folder inside the .app bundle
set(ASSETS_DESTINATION "${CMAKE_BINARY_DIR}/$<CONFIG>/Assets")

add_custom_command(
    OUTPUT "${ASSETS_DESTINATION}"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${ASSETS_DIR}" "${ASSETS_DESTINATION}"
    DEPENDS ${ASSET_FILES}
    COMMENT "Copying assets..."
)
endif()

add_custom_target(copy_assets ALL DEPENDS "${ASSETS_DESTINATION}")
add_dependencies(${EXECUTABLE_NAME} copy_assets)
