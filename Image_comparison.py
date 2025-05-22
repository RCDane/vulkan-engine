import numpy as np

def load_raw_buffer(path):
    with open(path, 'rb') as f:
        # read 3×4 bytes = 12 bytes header
        w = np.fromfile(f, dtype=np.uint32, count=1)[0]
        h = np.fromfile(f, dtype=np.uint32, count=1)[0]
        c = np.fromfile(f, dtype=np.uint32, count=1)[0]

        # now read w*h*c floats (16-bit)
        data = np.fromfile(f, dtype=np.float16, count=w*h*c)

    # reshape into (H, W, C)
    return data.reshape((h, w, c)), (w, h, c)

# Function to calculate Mean Squared Error (MSE)
def calculate_mse(image1, image2):
    return np.mean((image1 - image2) ** 2)

# Read the ground truth image
GT_image_path = "bin/image_0"
GT_img, (w_gt, h_gt, c_gt) = load_raw_buffer(GT_image_path)
GT_img = GT_img.astype(np.float32)

# Read and compare a set of images with the ground truth
N = 4  # Change this to the desired number of images
for i in range(1, N):
    img, (w, h, c) = load_raw_buffer(f'bin/image_{i}')
    print(f"Loaded image {w}×{h} with {c} channels; dtype={img.dtype}")

    # Convert to float32 for compatibility
    img = img.astype(np.float32)

    # Ensure the dimensions match the ground truth
    if (w, h, c) != (w_gt, h_gt, c_gt):
        print(f"Image {i} dimensions do not match the ground truth. Skipping comparison.")
        continue

    # Calculate MSE
    mse = calculate_mse(GT_img, img)
    print(f"Image {i} MSE with ground truth: {mse}")