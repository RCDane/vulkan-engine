import os
from convert_to_png import convert_raw_to_png

# Define input and output directories
input_dir = "python/data_images"
output_dir = "python/images"

# Ensure the output directory exists
os.makedirs(output_dir, exist_ok=True)

# Iterate over files in the input directory
for filename in os.listdir(input_dir):
    input_path = os.path.join(input_dir, filename)

    # Skip directories
    if os.path.isdir(input_path):
        continue

    # Define the output file name and path
    base_name, _ = os.path.splitext(filename)
    output_path = os.path.join(output_dir, f"{base_name}.png")

    try:
        # Convert the raw image to PNG
        convert_raw_to_png(input_path, output_path)
    except Exception as e:
        print(f"Failed to convert {filename}: {e}")
