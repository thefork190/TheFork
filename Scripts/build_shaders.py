import os
import subprocess
import sys

def process_shader_list(file_path, output_dir):
    command = [
        'python', 
        '../ThirdParty/The-Forge/Tools/ForgeShadingLanguage/fsl.py', 
        file_path,
        '-d', 'output_dir',
        '-b', 'output_bin',
        '-i', 'fsl_tmp',
        '-l DIRECT3D12',
        '--verbose',
        '--compile'
    ]
    
    try:
        result = subprocess.run(command, check=True, capture_output=True, text=True)
        print(f"Executed script on {file_path} successfully:\n{result.stdout}")
    except subprocess.CalledProcessError as e:
        print(f"Error executing script on {file_path}:\n{e.stderr}")

def search_and_process(input_dir, output_dir):
    for root, dirs, files in os.walk(input_dir):
        for file in files:
            if file == 'ShaderList.fsl':
                file_path = os.path.join(root, file)
                print(f"Found ShaderList.fsl: {file_path}")
                process_shader_list(file_path, output_dir)

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python build_shaders.py <input_directory> <output_directory>")
        sys.exit(1)

    input_dir = sys.argv[1]
    output_dir = sys.argv[2]

    if not os.path.isdir(input_dir):
        print(f"Input directory '{input_dir}' does not exist.")
        sys.exit(1)

    if not os.path.isdir(output_dir):
        print(f"Output directory '{output_dir}' does not exist. Creating it.")
        os.makedirs(output_dir)

    search_and_process(input_dir, output_dir)
