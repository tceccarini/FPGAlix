import sys, numpy as np, cv2

W, H = 640, 480

mode       = None
fullscreen = False
for arg in sys.argv[1:]:
    if arg == '-F':
        fullscreen = True
    elif arg in ('-r', '-d', '-dw', '-dwg'):
        mode = arg[1:]

if mode is None:
    print("Usage: view.py -r | -d | -dw | -dwg [-F]")
    print("  -r    raw Bayer as grayscale")
    print("  -d    demosaicing only")
    print("  -dw   demosaicing + white balance")
    print("  -dwg  demosaicing + white balance + gamma 2.2")
    print("  -F    fullscreen")
    sys.exit(1)

lut = np.array([((i / 255.0) ** (1 / 2.2)) * 255 for i in range(256)], dtype=np.uint8)

cv2.namedWindow('FPGAlix', cv2.WINDOW_NORMAL)
if not fullscreen:
    cv2.resizeWindow('FPGAlix', W, H)

first_frame = True

while True:
    data = sys.stdin.buffer.read(W * H)
    if len(data) < W * H:
        break
    raw = np.frombuffer(data, dtype=np.uint8).reshape(H, W)

    if mode == 'r':
        frame = raw.copy()
    else:
        frame = cv2.cvtColor(raw, cv2.COLOR_BayerRG2BGR_EA)
        if 'w' in mode:
            for i in range(3):
                lo, hi = np.percentile(frame[:, :, i], [1, 99])
                frame[:, :, i] = np.clip((frame[:, :, i].astype(np.float32) - lo) / (hi - lo) * 255, 0, 255).astype(np.uint8)
        if mode.endswith('g'):
            frame = cv2.LUT(frame, lut)

    text_color = 255 if mode == 'r' else (0, 255, 0)
    cv2.putText(frame, f"-{mode}", (10, 20), cv2.FONT_HERSHEY_SIMPLEX, 0.5, text_color, 1)
    cv2.imshow('FPGAlix', frame)
    if fullscreen and first_frame:
        cv2.setWindowProperty('FPGAlix', cv2.WND_PROP_FULLSCREEN, cv2.WINDOW_FULLSCREEN)
        first_frame = False
    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

cv2.destroyAllWindows()
