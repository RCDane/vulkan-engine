from PIL import Image
import os
import numpy as np
def crop_images(images, crop_box):

    cropped_images = []

    for img in images:
        try:
            cropped_img = img.crop(crop_box)
            cropped_images.append(cropped_img)
        except Exception as e:
            print(f"Failed to crop image: {e}")

    return cropped_images

def load_png_image(file_path):

    try:
        with Image.open(file_path) as img:
            return img.copy()
    except Exception as e:
        print(f"Failed to load image from {file_path}: {e}")
        return None
    
    
imagePath = "python/images/"    

image1 = load_png_image(imagePath + "snip_svgf.png")
image2 = load_png_image(imagePath + "snip_acc.png")
# image3 = load_png_image(imagePath + "accumelated_0.png")
# image4 = load_png_image(imagePath + "accumelated_no_indirect_0.png")


imgs = [image1, image2]
crop_box = (340, 100, 900, 800)  # Define the crop box (left, upper, right, lower)
cropped_images = crop_images(imgs, crop_box)

# # convert crops to grayscale and compute signed diff
# arr1 = np.array(cropped_images[0].convert("L"), dtype=np.float32)
# arr2 = np.array(cropped_images[1].convert("L"), dtype=np.float32)
# diff = arr1 - arr2
# max_abs = np.max(np.abs(diff)) or 1.0  # prevent zero division

# plot original images in one figure
import matplotlib.pyplot as plt
fig1, axs1 = plt.subplots(1, 2, figsize=(8, 4))
axs1[0].imshow(cropped_images[0])
axs1[0].axis("off")
axs1[1].imshow(cropped_images[1])
axs1[1].axis("off")
plt.tight_layout()
plt.show()

# # plot diff in a separate figure
# fig2, ax2 = plt.subplots(figsize=(6, 4))
# im = ax2.imshow(diff, cmap="seismic", vmin=-max_abs/4, vmax=max_abs/4)
# ax2.axis("off")
# fig2.colorbar(im, ax=ax2, shrink=0.7)
# plt.tight_layout()
# plt.show()