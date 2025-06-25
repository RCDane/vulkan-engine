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

    # Add raw data analysis and validation before processing
    # Debug: inspect raw data and file integrity
    file_size = os.path.getsize(input_path)
    # Use float32 item size since raw data is 32-bit floats
    expected_size = 12 + (w * h * c) * np.dtype(np.float32).itemsize
    print(f"Raw file size: {file_size} bytes, expected: {expected_size} bytes")
    # Validate pixel count
    expected_count = w * h * c
    actual_count = img.size
    if actual_count != expected_count:
        print(f"Warning: expected {expected_count} values, but loaded {actual_count}")
    # Compute basic statistics
    nan_count = np.isnan(img).sum()
    posinf_count = np.isposinf(img).sum()
    neginf_count = np.isneginf(img).sum()
    try:
        raw_min = np.nanmin(img)
        raw_max = np.nanmax(img)
        raw_mean = np.nanmean(img)
    except ValueError:
        raw_min = raw_max = raw_mean = float('nan')
    print(f"Raw data stats -> min: {raw_min}, max: {raw_max}, mean: {raw_mean}, NaNs: {nan_count}, +inf: {posinf_count}, -inf: {neginf_count}")

    # sanitize NaNs and infs in one go
    img = np.nan_to_num(img, nan=0.0, posinf=0.0, neginf=0.0)
    # clamp negative values to zero (black)
    img[img < 0] = 0
    # clamp extreme high values to 99th percentile to reduce outliers
    high = np.nanpercentile(img, 99)
    img[img > high] = high

    # Normalize the image to 0–255
    max_val = img.max()
    if max_val > 0:
        img = img / max_val * 255
    else:
        img = img * 0
    img = img.astype(np.uint8)
    # if RGBA, force alpha channel to fully opaque
    if c == 4:
        img[..., 3] = 255

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