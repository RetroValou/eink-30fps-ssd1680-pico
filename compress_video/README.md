# Video to E-Paper Header Converter

A Python utility to convert MP4 video files into a C header file (`video_data.h`) using Delta RLE compression. Designed for real-time video rendering on SSD1680 E-Paper displays driven by a Raspberry Pi Pico.

The script processes video frames through contrast enhancement (CLAHE), optional rotation/scaling, binarization (or temporal grayscale simulation), and packs them into a custom 4-command Delta RLE stream stored in C `#pragma` Flash memory arrays (`.rodata`).

## Requirements
* uv Python package manager installed.
* Dependencies: `opencv-python`, `numpy` (automatically managed by uv).

## How to Run

1. Place your video files in ./video/.
2. Edit the function parameters in main.py if needed (video path, screen resolution, rotation, grayscale simulation).
3. Execute using uv:
```bash
uv run main.py
```

The output C header file (`video_data.h`) will be generated at the configured path, ready to be included in your C/C++ Raspberry Pi Pico SDK project.