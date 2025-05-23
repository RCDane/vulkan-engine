import numpy as np
from PIL import Image
from util import load_raw_buffer
import argparse
import os

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


if __name__ == "__main__":
    # Parse command-line arguments
    parser = argparse.ArgumentParser(description="Convert a raw image to PNG format.")
    parser.add_argument("input_path", type=str, help="Path to the input raw image file.")
    parser.add_argument("output_dir", type=str, help="Directory to save the output PNG file.")
    parser.add_argument("new_name", type=str, help="New name for the output PNG file (without extension).")
    args = parser.parse_args()

    # Construct the full output path
    output_path = os.path.join(args.output_dir, f"{args.new_name}.png")

    # Convert the raw image to PNG
    convert_raw_to_png(args.input_path, output_path)