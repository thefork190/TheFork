@echo off
cd /d "%~dp0"
python build_shaders.py ../Assets ../scripts_output/FSL/source ../scripts_output/FSL/binary ../scripts_output/FSL/tmp "DIRECT3D12 VULKAN"