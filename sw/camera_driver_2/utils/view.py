import sys, numpy as np, cv2

W, H = 640, 480
while True:
    data = sys.stdin.buffer.read(W * H)
    if len(data) < W * H:
        break
    frame = np.frombuffer(data, dtype=np.uint8).reshape(H, W)
    cv2.imshow('FPGAlix', frame)
    if cv2.waitKey(1) & 0xFF == ord('q'):
        break
cv2.destroyAllWindows()
