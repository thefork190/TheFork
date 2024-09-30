@echo off
cd /d "%~dp0"
python build_shaders.py ../Source/FSL ../scripts_output/FSL/source ../Assets/FSL/binary ../scripts_output/FSL/tmp "ANDROID_VULKAN"
python build_shaders.py ../ThirdParty/The-Forge/RHI/FSL ../scripts_output/FSL/source ../Assets/FSL/binary ../scripts_output/FSL/tmp "ANDROID_VULKAN"