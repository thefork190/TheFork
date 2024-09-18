#!/bin/sh 

cd "$(dirname "$0")"
python build_shaders.py ../Source/FSL ../scripts_output/FSL/source ../Assets/FSL/binary ../scripts_output/FSL/tmp "MACOS"
python build_shaders.py ../ThirdParty/The-Forge/RHI/FSL ../scripts_output/FSL/source ../Assets/FSL/binary ../scripts_output/FSL/tmp "MACOS"