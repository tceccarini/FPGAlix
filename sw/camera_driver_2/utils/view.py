import sys, numpy as np, cv2

W, H = 640, 480

PATTERNS = [
    (cv2.COLOR_BayerBG2BGR, "BayerBG (BGGR - datasheet)"),
    (cv2.COLOR_BayerGB2BGR, "BayerGB (GBRG - upstream driver)"),
    (cv2.COLOR_BayerRG2BGR, "BayerRG (RGGB)"),
    (cv2.COLOR_BayerGR2BGR, "BayerGR (GRBG)"),
]

idx = int(sys.argv[1]) if len(sys.argv) > 1 else 3
code, name = PATTERNS[idx]
print(f"Pattern: {name}  (pass 0-3 as argument to change)")

while True:
    data = sys.stdin.buffer.read(W * H)
    if len(data) < W * H:
        break
    raw   = np.frombuffer(data, dtype=np.uint8).reshape(H, W)
    frame = cv2.cvtColor(raw, code)
    cv2.putText(frame, name, (10, 20), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 0), 1)
    cv2.imshow('FPGAlix', frame)
    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

cv2.destroyAllWindows()
