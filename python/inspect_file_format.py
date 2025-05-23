import os
import numpy as np

def inspect_file(path):
    try:
        print(f"Inspecting file: {path}")
        with open(path, 'rb') as f:
            # Read the first 12 bytes (header)
            header = f.read(12)
            print(f"Header bytes: {header}")
            w, h, c = np.frombuffer(header, dtype=np.uint32)

            print(f"Width: {w}, Height: {h}, Channels: {c}")

            # Read the rest of the file
            data = f.read()
            print(f"Data size: {len(data)} bytes")
    except Exception as e:
        print(f"Failed to inspect file {path}: {e}")

# Path to the file to inspect
file_path = "python/data_images/sponza_accumelate_lights_0"
inspect_file(file_path)
