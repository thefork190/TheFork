import os
import subprocess
import sys

def process_shader_list(file_path, output_dir, bin_dir, tmp_dir, platforms, root_dir):
    # Determine relative path from the root input directory
    relative_path = os.path.relpath(file_path, root_dir)

    # Calculate the new output and binary paths preserving the directory structure
    new_output_dir = os.path.join(output_dir, os.path.dirname(relative_path))
    new_bin_dir = os.path.join(bin_dir, os.path.dirname(relative_path))

    # Create directories if they don't exist
    os.makedirs(new_output_dir, exist_ok=True)
    os.makedirs(new_bin_dir, exist_ok=True)

    command = [
        'python',
        '../ThirdParty/The-Forge/Tools/ForgeShadingLanguage/fsl.py',
        file_path,
        '-d', new_output_dir,
        '-b', new_bin_dir,
        '-i', tmp_dir,
        f'-l {platforms}',
        '--verbose',
        '--compile'
    ]

    try:
        result = subprocess.run(command, check=True, capture_output=True, text=True)
        print(f"Executed script on {file_path} successfully:\n{result.stdout}")
    except subprocess.CalledProcessError as e:
        print(f"Error executing script on {file_path}:\n{e.stderr}")

def search_and_process(input_dir, output_dir, bin_dir, tmp_dir, platforms):
    for root, dirs, files in os.walk(input_dir):
        for file in files:
            if file == 'ShaderList.fsl':
                file_path = os.path.join(root, file)
                print(f"Found ShaderList.fsl: {file_path}")
                process_shader_list(file_path, output_dir, bin_dir, tmp_dir, platforms, input_dir)

if __name__ == "__main__":
    if len(sys.argv) != 6:
        print("Usage: python build_shaders.py <input_dir> <output_dir> <binary_output_dir> <tmp_dir> <platforms>")
        sys.exit(1)

    input_dir = sys.argv[1]
    output_dir = sys.argv[2]
    bin_dir = sys.argv[3]
    tmp_dir = sys.argv[4]
    platforms = sys.argv[5]

    # Ensure directories exist, create if not
    for dir_path in [input_dir, tmp_dir]:
        if not os.path.isdir(dir_path):
            print(f"Directory '{dir_path}' does not exist. Creating it.")
            os.makedirs(dir_path)

    search_and_process(input_dir, output_dir, bin_dir, tmp_dir, platforms)
