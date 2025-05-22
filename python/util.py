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