from PIL import Image
import os

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

image1 = load_png_image(imagePath + "pillars_accum_0.png")
image2 = load_png_image(imagePath + "pillars_noisy_0.png")
image3 = load_png_image(imagePath + "pillars_svgf_0.png")


imgs = [image1, image2, image3]
crop_box = (500, 200, 850, 650)  # Define the crop box (left, upper, right, lower)
cropped_images = crop_images(imgs, crop_box)

# plot using matplotlib
import matplotlib.pyplot as plt
for i, cropped_img in enumerate(cropped_images):
    plt.subplot(1, len(cropped_images), i + 1)
    plt.imshow(cropped_img)
    plt.axis('off')

plt.show()