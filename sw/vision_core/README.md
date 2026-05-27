# vision_core

Real-time RTSP streaming pipeline for the FPGAlix board (Intel Cyclone V SoC).
Captures frames from a V4L2 device, runs a user-configurable filter pipeline, and
streams the result over RTSP using GStreamer.

## Architecture

```
V4L2 device
    │
    ▼
FilteredCapturer          (capture thread)
    │  raw frame
    ▼
Filter pipeline           (user-defined, hot-swappable via SimpleUI)
    │
    ▼
FilterConversion          (automatic final step — converts to FrameBuffer format)
    │
    ▼
FrameBuffer               (lock-free pool, 4 frames)
    │
    ▼
Streamer                  (tx thread + GMainLoop thread)
    │
    ▼
RTSP server  →  rtsp://0.0.0.0:8554/stream
```

### Supported sources

| Source | Class | Resolution | FPS | Raw format |
|--------|-------|-----------|-----|-----------|
| OV7670 via Altera mSGDMA | `OV7670FilteredCapturer` | 640×480 | 30 | Bayer CV_8UC1 |
| Generic V4L2 webcam | `WebCamFilteredCapturer` | negotiated | negotiated | BGR / YUYV |

### Encoding / format combinations

| Encoding | Buffer format | Notes |
|----------|--------------|-------|
| MJPEG | BGR8 | `cv::imencode` → `rtpjpegpay` |
| UNCOMPRESSED | BGR8 | zero-copy wrap → `rtpvrawpay` (RFC 4175) |
| H264 | GRAY8 | zero-copy → `videoconvert` → `openh264enc` — lightest |
| H264 | BGR8 | zero-copy → `videoconvert` (BGR→I420) → `openh264enc` |

If the user selects a format not supported by the chosen encoding, `Main.cpp`
automatically falls back to the least-costly available format and informs the user.

### Filter pipeline (SimpleUI)

At runtime, type space-separated filter tokens at the prompt:

```
awb demosaicing:bg:bilinear gamma:2.2:1.0
```

Available filters:

| Token | Description |
|-------|-------------|
| `none` | Clear all filters |
| `awb` / `awb:<clip%>` | Auto white balance (BGR only) |
| `gamma:<γ>:<gain>` | Gamma + gain, any CV_8U depth |
| `demosaicing:<pat>:<algo>` | Bayer demosaicing — pat: `bg\|gb\|rg\|gr`, algo: `bilinear\|vng\|ea\|gray` |
| `conversion:<fmt>` | Explicit colorspace conversion — fmt: `bgr8\|gray8` |
| `keepmosaic:<pat>:<fmt>` | Keep raw Bayer pattern in output |
| `sobel` | Edge magnitude (CV_8UC1) |
| `sobel:overlay` | Green edges overlaid on original |

The pipeline is double-buffered: edits take effect on the next captured frame
without dropping frames or stalling the capture thread.

---

## Building

Requires a C++17 toolchain, OpenCV 4, and GStreamer 1.x with the RTSP server library.

```bash
make        # builds bin/vision_core
make clean
```

Dependencies are resolved via `pkg-config`:
- `opencv4`
- `gstreamer-1.0`
- `gstreamer-rtsp-server-1.0`
- `gstreamer-app-1.0`

---

## Buildroot configuration (Cyclone V SoC)

### Toolchain

```
Toolchain
  → Target Architecture Variant          cortex-a9
  → Floating point strategy              NEON          (-mfpu=neon -mfloat-abi=hard)
  → ARM instruction set                  ARM           (not Thumb — better for SIMD)
  → Target ABI                           EABIhf
```

### Build options

```
Build options
  → Optimization level                   -O2
```

`-O3` can help on float-heavy code (gamma, AWB) but occasionally miscompiles
NEON intrinsics on ARMv7 — test before shipping.

### Required packages

#### OpenCV

```
Target packages → Libraries → Graphics → opencv4
```

| Option | Value |
|--------|-------|
| `imgcodecs` | ✔` |
| `imgproc` | `✔ |
| `videoio` | ✔ |
| `jpeg support` | ✔ — required for `cv::imencode` (MJPEG path) |
| `v4l support` | ✔ — required for `CAP_V4L2` |
| `tbb support` |✔ — enables multi-core `parallel_for_` |

NEON is auto-detected by OpenCV's cmake when the toolchain sets `-mfpu=neon`.
Confirm `NEON: YES` in the cmake build log.

#### GStreamer

```
Target packages → Audio and video applications → gstreamer 1.x
```

| Package | Sub-plugin to enable | Provides |
|---------|---------------------|---------|
| `gstreamer1` | — | GStreamer core |
| `gst1-plugins-base` | `app` | `appsrc` / `appsink` |
| `gst1-plugins-base` | `videoconvertscale` | `videoconvert` (default on) |
| `gst1-plugins-good` | `rtpmanager` (not `rtp`) | `rtpjpegpay`, `rtpvrawpay`, `rtph264pay` |
| `gst1-plugins-bad` | `openh264` | `openh264enc` (H264 only) |
| `gst1-rtsp-server` | — | `GstRTSPServer` API |

#### libopenh264 (H264 encoding only)

```
Target packages → Libraries → Multimedia → libopenh264
```

Required by the `openh264` plugin inside `gst1-plugins-bad`. If H264 is not
needed, skip both this package and the `openh264` sub-plugin.

#### libjpeg-turbo (strongly recommended for MJPEG)

```
Target packages → Libraries → Graphics → libjpeg-turbo
```

Drop-in replacement for libjpeg with NEON-accelerated DCT kernels.
Reduces MJPEG encode time by roughly 3× on Cortex-A9 compared to plain libjpeg.
Make sure OpenCV links against it (it will if it is the only JPEG library present).

---

## Optimizations for Cyclone V (Cortex-A9)

### NEON SIMD
The Cortex-A9 integrates a 128-bit NEON unit (VFPv3-D32). The following
operations use NEON automatically when OpenCV is built with `-mfpu=neon`:

- `cv::cvtColor` (BGR↔GRAY, Bayer demosaicing)
- `cv::Sobel` (imgproc convolution engine)
- `cv::LUT` (gamma correction)
- `cv::imencode` JPEG (via libjpeg-turbo)

### Multi-core
The HPS has two Cortex-A9 cores. OpenCV's `parallel_for_` distributes heavy
imgproc operations across both. Enable pthreads parallelism in OpenCV:

```
opencv4 → With pthreads-based parallelism   ✔
```

Or TBB if you prefer a work-stealing scheduler, but pthreads is lighter for
an embedded target with only two cores.

### H264 on Cyclone V — manage expectations
The HPS has no hardware video encoder. `openh264enc` runs entirely on the
Cortex-A9 in software. At 640×480 @ 30 fps:

- **H264 GRAY8** is feasible — skips chroma plane, lower encoder effort.
- **H264 BGR8** is demanding — BGR→I420 colorspace conversion adds overhead
  on top of encoding. Profile first with `top` or `perf`.
- **MJPEG** is the recommended choice for full-color streaming on this target:
  fast with libjpeg-turbo NEON, no encoder state, low latency.

### Zero-copy V4L2 (advanced)
OpenCV's `VideoCapture` with `CAP_V4L2` performs a kernel→userspace copy for
each frame. If the OV7670 V4L2 driver supports `V4L2_MEMORY_MMAP` buffers,
replacing `cv::VideoCapture` with a direct `ioctl`-based capture loop in
`OV7670FilteredCapturer::openDevice` eliminates that copy.
