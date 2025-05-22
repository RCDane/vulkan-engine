import numpy as np
from PIL import Image
from util import load_raw_buffer
import argparse
import os
import glob

def convert_raw_to_png(input_path, output_path):
    # Load the raw image using the utility function
    img, (w, h, c) = load_raw_buffer(input_path)
    
    # Convert to float32 for compatibility
    img = img.astype(np.float32)
    
    # Handle different channel formats
    if c == 1:  # Grayscale
        img = img[:, :, 0]  # Remove the channel dimension
        mode = "L"  # 8-bit pixels, grayscale
    elif c == 3:  # RGB
        img = img[:, :, :3]  # Ensure only 3 channels are used
        mode = "RGB"
    elif c == 4:  # RGBA
        img = img[:, :, :4]  # Ensure only 4 channels are used
        mode = "RGBA"
    else:
        raise ValueError(f"Unsupported number of channels: {c}")

    # Normalize the image to 0-255 and convert to uint8
    img = (img / np.max(img) * 255).astype(np.uint8)

    # Save the image as PNG
    Image.fromarray(img, mode).save(output_path)
    print(f"Saved PNG image to {output_path}")


def batch_convert_raw_to_png(input_dir, output_dir):
    # Find all files in the input directory
    raw_files = glob.glob(os.path.join(input_dir, '*'))

    for raw_file in raw_files:
        # Generate a new name for the output PNG file based on the input file name
        base_name = os.path.splitext(os.path.basename(raw_file))[0]
        output_path = os.path.join(output_dir, f"{base_name}.png")

        try:
            # Convert the raw image to PNG
            convert_raw_to_png(raw_file, output_path)
        except Exception as e:
            print(f"Failed to convert {raw_file}: {e}")


if __name__ == "__main__":
    # Parse command-line arguments
    parser = argparse.ArgumentParser(description="Convert raw images in a directory to PNG format.")
    parser.add_argument("input_dir", type=str, help="Path to the input directory containing raw image files.")
    parser.add_argument("output_dir", type=str, help="Directory to save the output PNG files.")
    args = parser.parse_args()

    # Convert all raw images in the input directory to PNG
    batch_convert_raw_to_png(args.input_dir, args.output_dir)