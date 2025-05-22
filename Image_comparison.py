import numpy as np
import matplotlib.pyplot as plt  # Added import for matplotlib

def load_raw_buffer(path):
    with open(path, 'rb') as f:
        # read 3×4 bytes = 12 bytes header
        w = np.fromfile(f, dtype=np.uint32, count=1)[0]
        h = np.fromfile(f, dtype=np.uint32, count=1)[0]
        c = np.fromfile(f, dtype=np.uint32, count=1)[0]

        # now read w*h*c floats
        data = np.fromfile(f, dtype=np.float32, count=w*h*c)

    # reshape into (H, W, C)
    return data.reshape((h, w, c)), (w, h, c)

# example usage:
img, (w, h, c) = load_raw_buffer('bin/test')
print(f"Loaded image {w}×{h} with {c} channels; dtype={img.dtype}")

# Format and display the image using matplotlib
if c == 1:  # Grayscale image
    img = img[:, :, 0]  # Remove the channel dimension
elif c == 3:  # RGB image
    img = img[:, :, :3]  # Ensure only 3 channels are used
elif c == 4:  # RGBA image
    img = img[:, :, :4]  # Ensure only 4 channels are used

plt.imshow(img, cmap='gray' if c == 1 else None)
plt.title(f"Image {w}×{h} with {c} channels")
plt.axis('off')
plt.show()