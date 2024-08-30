import os
import subprocess
import sys

def process_shader_list(file_path, output_dir, bin_dir, tmp_dir):
    command = [
        'python', 
        '../ThirdParty/The-Forge/Tools/ForgeShadingLanguage/fsl.py', 
        file_path,
        '-d', output_dir,
        '-b', bin_dir,
        '-i', tmp_dir,
        '-l DIRECT3D12 VULKAN', # TODO pass in arg to dynamically set this based on platform
        '--verbose',
        '--compile'
    ]
    
    try:
        result = subprocess.run(command, check=True, capture_output=True, text=True)
        print(f"Executed script on {file_path} successfully:\n{result.stdout}")
    except subprocess.CalledProcessError as e:
        print(f"Error executing script on {file_path}:\n{e.stderr}")

def search_and_process(input_dir, output_dir, bin_dir, tmp_dir):
    for root, dirs, files in os.walk(input_dir):
        for file in files:
            if file == 'ShaderList.fsl':
                file_path = os.path.join(root, file)
                print(f"Found ShaderList.fsl: {file_path}")
                process_shader_list(file_path, output_dir, bin_dir, tmp_dir)

if __name__ == "__main__":
    if len(sys.argv) != 5:
        print("Usage: python build_shaders.py <input_dir> <output_dir> <binary_output_dir> <tmp_dir>")
        sys.exit(1)

    input_dir = sys.argv[1]
    output_dir = sys.argv[2]
    bin_dir = sys.argv[3]
    tmp_dir = sys.argv[4]

    if not os.path.isdir(input_dir):
        print(f"Input directory '{input_dir}' does not exist.")
        sys.exit(1)

    if not os.path.isdir(output_dir):
        print(f"Output directory '{output_dir}' does not exist. Creating it.")
        os.makedirs(output_dir)

    if not os.path.isdir(bin_dir):
        print(f"Output directory '{bin_dir}' does not exist. Creating it.")
        os.makedirs(bin_dir)

    if not os.path.isdir(tmp_dir):
        print(f"Output directory '{tmp_dir}' does not exist. Creating it.")
        os.makedirs(tmp_dir)

    search_and_process(input_dir, output_dir, bin_dir, tmp_dir)
