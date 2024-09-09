import os
import subprocess
import sys

def process_shader_list(file_path, output_dir, bin_dir, tmp_dir, platforms, root_dir, debug):
    # Ensure root_dir is absolute
    root_dir = os.path.abspath(root_dir)
    
    # Determine relative path from the root input directory
    relative_path = os.path.relpath(file_path, root_dir)

    # Calculate the new output and binary paths preserving the directory structure
    new_output_dir = os.path.join(output_dir, os.path.dirname(relative_path))
    
    # TODO: right now we dump all bins in same dir so it's easier to load with TF's resource loader
    # new_bin_dir = os.path.join(bin_dir, os.path.dirname(relative_path))
    new_bin_dir = bin_dir

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

    if debug:
        command.append('--debug')

    try:
        result = subprocess.run(command, check=True, capture_output=True, text=True)
        print(f"Executed script on {file_path} successfully:\n{result.stdout}")
    except subprocess.CalledProcessError as e:
        print(f"Error executing script on {file_path}:\n{e.stderr}")
        print(f"Output from the command:\n{e.stdout}")

def search_and_process(input_dir, output_dir, bin_dir, tmp_dir, platforms, debug):
    # Ensure directories are absolute
    input_dir = os.path.abspath(input_dir)
    output_dir = os.path.abspath(output_dir)
    bin_dir = os.path.abspath(bin_dir)
    tmp_dir = os.path.abspath(tmp_dir)

    for root, dirs, files in os.walk(input_dir):
        for file in files:
            if file == 'ShaderList.fsl':
                file_path = os.path.join(root, file)
                print(f"Found ShaderList.fsl: {file_path}")
                process_shader_list(file_path, output_dir, bin_dir, tmp_dir, platforms, input_dir, debug)

if __name__ == "__main__":
    if len(sys.argv) < 6 or len(sys.argv) > 7:
        print("Usage: python build_shaders.py <input_dir> <output_dir> <binary_output_dir> <tmp_dir> <platforms> [--debug]")
        sys.exit(1)

    # Convert command-line arguments to absolute paths
    input_dir = os.path.abspath(sys.argv[1])
    output_dir = os.path.abspath(sys.argv[2])
    bin_dir = os.path.abspath(sys.argv[3])
    tmp_dir = os.path.abspath(sys.argv[4])
    platforms = sys.argv[5]

    # Check if debug flag is set
    debug = len(sys.argv) == 7 and sys.argv[6] == '--debug'

    # Ensure directories exist, create if not
    for dir_path in [input_dir, tmp_dir]:
        if not os.path.isdir(dir_path):
            print(f"Directory '{dir_path}' does not exist. Creating it.")
            os.makedirs(dir_path)

    search_and_process(input_dir, output_dir, bin_dir, tmp_dir, platforms, debug)
