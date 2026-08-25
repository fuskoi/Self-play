from PIL import Image
import os

def remove_white_bg(img, threshold=245, soft=18):
    img = img.convert("RGBA")
    pixels = img.load()
    w, h = img.size
    for y in range(h):
        for x in range(w):
            r, g, b, a = pixels[x, y]
            if r >= threshold and g >= threshold and b >= threshold:
                pixels[x, y] = (r, g, b, 0)
            elif r >= threshold - soft and g >= threshold - soft and b >= threshold - soft:
                avg = (r + g + b) / 3.0
                alpha = int(max(0, min(255, (threshold - avg) / soft * 255)))
                pixels[x, y] = (r, g, b, alpha)
    return img


def trim_transparent(img, pad=4):
    bbox = img.getbbox()
    if not bbox:
        return img
    l, t, r, b = bbox
    l = max(0, l - pad)
    t = max(0, t - pad)
    r = min(img.width, r + pad)
    b = min(img.height, b + pad)
    return img.crop((l, t, r, b))


work_path = r"d:\QtProject\countdown\resources\mascot.png"
rest_path = r"d:\QtProject\countdown\resources\mascot_rest.png"
backup_work = r"d:\QtProject\countdown\resources\mascot_orig.png"
backup_rest = r"d:\QtProject\countdown\resources\mascot_rest_orig.png"

if not os.path.exists(backup_work):
    Image.open(work_path).save(backup_work)
if not os.path.exists(backup_rest):
    Image.open(rest_path).save(backup_rest)

# Prefer originals if present (re-run safe)
src_work = backup_work if os.path.exists(backup_work) else work_path
src_rest = backup_rest if os.path.exists(backup_rest) else rest_path

work = remove_white_bg(Image.open(src_work), threshold=242, soft=20)
work = trim_transparent(work)
side = max(work.width, work.height)
canvas = Image.new("RGBA", (side, side), (0, 0, 0, 0))
canvas.paste(work, ((side - work.width) // 2, (side - work.height) // 2), work)
canvas.save(work_path)
print("work saved", canvas.size, canvas.mode)

rest = Image.open(src_rest)
w, h = rest.size
left = int(w * 0.55)
rest_crop = rest.crop((left, 0, w, h))
rest_crop = remove_white_bg(rest_crop, threshold=240, soft=22)
rest_crop = trim_transparent(rest_crop)
side = max(rest_crop.width, rest_crop.height)
canvas2 = Image.new("RGBA", (side, side), (0, 0, 0, 0))
canvas2.paste(rest_crop, ((side - rest_crop.width) // 2, (side - rest_crop.height) // 2), rest_crop)
canvas2.save(rest_path)
print("rest saved", canvas2.size, canvas2.mode)
print("done")
