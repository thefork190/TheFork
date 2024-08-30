import os
import subprocess
import sys

def process_shader_list(file_path, output_dir, bin_dir, tmp_dir, platforms):
    command = [
        'python',
        '../ThirdParty/The-Forge/Tools/ForgeShadingLanguage/fsl.py',
        file_path,
        '-d', output_dir,
        '-b', bin_dir,
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
                process_shader_list(file_path, output_dir, bin_dir, tmp_dir, platforms)

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
    for dir_path in [input_dir, output_dir, bin_dir, tmp_dir]:
        if not os.path.isdir(dir_path):
            print(f"Directory '{dir_path}' does not exist. Creating it.")
            os.makedirs(dir_path)

    search_and_process(input_dir, output_dir, bin_dir, tmp_dir, platforms)
