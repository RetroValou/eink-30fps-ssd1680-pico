import cv2
import numpy as np

import cv2
import numpy as np



def compress_video_to_header(input_file, output_file="video_data.h", width=128, height=296, fps_target=30, 
                             max_rom_bytes=1.8 * 1024 * 1024, horizontal_squeeze=1.0, rotate_90=True, 
                             simulate_gray=False, dark_thresh=60, light_thresh=190,
                             scale_factor=1.0, bg_color=255):
    """
    Compresses a video file into a C header file (.h) containing Delta RLE encoded 1-bit frames
    optimized for SSD1680 E-Paper displays on Raspberry Pi Pico.

    Parameters:
        - input_file : Path to the input video file.
        - output_file : Output path for the generated C header file (default: "video_data.h").
        - width : Display width in pixels (default: 128).
        - height : Display height in pixels (default: 296).
        - fps_target : Target playback frame rate written to header metadata (default: 30).
        - max_rom_bytes : Maximum flash memory limit in bytes (default: ~1.8 MB for RP2040 Flash).
        - horizontal_squeeze : Aspect ratio correction factor before resizing (default: 1.0).
        - rotate_90 : Rotates input frames 90 degrees clockwise for landscape display (default: True).
        - simulate_gray : Enables 2-bit grayscale simulation via frame-by-frame temporal pixel blinking (default: False).
        - dark_thresh : Luminance cutoff below which pixels are forced black in grayscale mode (default: 60).
        - light_thresh : Luminance cutoff above which pixels are forced white in grayscale mode (default: 190).
        - scale_factor : Additional scale modifier for cover-fit aspect resizing (default: 1.0).
        - bg_color : Background padding color: 255 for White, 0 for Black (default: 255).
    """
    
    cap = cv2.VideoCapture(input_file)
    total_video_frames = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))
    
    total_bytes = (width * height) // 8
    prev_bytes = np.zeros(total_bytes, dtype=np.uint8)
    
    all_frames = []
    total_compressed_size = 0
    started = False
    skipped_frames = 0
    
    print(f"Starting V3 Compression (Delta RLE + FILL). Flash limit: {max_rom_bytes / 1024:.2f} KB")
    if simulate_gray: print(f"Grayscale simulation ENABLED. Dark thresh: <{dark_thresh}, Light thresh: >{light_thresh}")
    print(f"Full Screen Cover ENABLED. Scale factor: {scale_factor * 100:.0f}%, BG Color: {'White' if bg_color == 255 else 'Black'}")
    
    while cap.isOpened():
        ret, frame = cap.read()
        if not ret: break
            
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        
        #  --- ROTATION ---
        if rotate_90: working_frame = cv2.rotate(gray, cv2.ROTATE_90_CLOCKWISE)
        else: working_frame = gray
            
        in_h, in_w = working_frame.shape
        
        #  --- Mirror + accentuate contrast ---
        mirrored = cv2.flip(working_frame, 1)
        clahe = cv2.createCLAHE(clipLimit=3.0, tileGridSize=(8, 8))
        enhanced = clahe.apply(mirrored)

        #  --- ASPECT COVER  --- 
        eff_in_w = in_w * horizontal_squeeze
        
        base_scale = max(width / eff_in_w, height / in_h)
        scale = base_scale * scale_factor

        inner_w = max(1, int(eff_in_w * scale))
        inner_h = max(1, int(in_h * scale))

        resized_video = cv2.resize(enhanced, (inner_w, inner_h))
        final_frame = np.full((height, width), bg_color, dtype=np.uint8)

        # Calculate cropping boundaries
        src_x1 = max(0, (inner_w - width) // 2)
        src_x2 = src_x1 + min(inner_w, width)
        src_y1 = max(0, (inner_h - height) // 2)
        src_y2 = src_y1 + min(inner_h, height)

        dst_x1 = max(0, (width - inner_w) // 2)
        dst_x2 = dst_x1 + min(inner_w, width)
        dst_y1 = max(0, (height - inner_h) // 2)
        dst_y2 = dst_y1 + min(inner_h, height)

        final_frame[dst_y1:dst_y2, dst_x1:dst_x2] = resized_video[src_y1:src_y2, src_x1:src_x2]

        #  --- Binarization of image ---
        if simulate_gray: # simulation of grey -> blink pixel from black to white
            bw = np.zeros_like(final_frame, dtype=np.uint8)
            
            bw[final_frame > light_thresh] = 1 # transform to white
            
            if len(all_frames) % 2 == 0: # gray: pixel blinking
                bw[(final_frame >= dark_thresh) & (final_frame <= light_thresh)] = 1
                
            curr_bytes = np.packbits(bw.flatten())

        else: # mode black and white
            _, bw = cv2.threshold(final_frame, 127, 1, cv2.THRESH_BINARY)
            curr_bytes = np.packbits(bw.flatten())
        
        if not started:
            if np.all(curr_bytes == 0) or np.all(curr_bytes == 255):
                skipped_frames += 1
                continue
            else:
                print(f"First non-uniform frame detected at frame {skipped_frames}. Starting encoding.")
                started = True

        #  --- DELTA RLE 4-COMMAND ENCODING (Compression of video) ---
        compressed = []
        i = 0
        while i < total_bytes:
            # 1. SKIP
            match_len = 0
            while i + match_len < total_bytes and match_len < 63 and curr_bytes[i + match_len] == prev_bytes[i + match_len]:
                match_len += 1
                
            if match_len >= 1:
                if i + match_len < total_bytes: compressed.append(match_len) 
                i += match_len
                continue
                
            # 2. FILL 0x00 (Black)
            fill0_len = 0
            while i + fill0_len < total_bytes and fill0_len < 63 and curr_bytes[i + fill0_len] == 0x00:
                fill0_len += 1
                
            if fill0_len >= 2:
                compressed.append(0x80 | fill0_len)
                i += fill0_len
                continue

            # 3. FILL 0xFF (White)
            fill1_len = 0
            while i + fill1_len < total_bytes and fill1_len < 63 and curr_bytes[i + fill1_len] == 0xFF:
                fill1_len += 1
                
            if fill1_len >= 2:
                compressed.append(0xC0 | fill1_len)
                i += fill1_len
                continue
                
            # 4. LITERAL
            lit_len = 0
            while i + lit_len < total_bytes and lit_len < 63:
                if i + lit_len + 1 < total_bytes:
                    if curr_bytes[i + lit_len] == prev_bytes[i + lit_len] and curr_bytes[i + lit_len + 1] == prev_bytes[i + lit_len + 1]:
                        break
                    if curr_bytes[i + lit_len] == 0x00 and curr_bytes[i + lit_len + 1] == 0x00:
                        break
                    if curr_bytes[i + lit_len] == 0xFF and curr_bytes[i + lit_len + 1] == 0xFF:
                        break
                lit_len += 1
                
            compressed.append(0x40 | lit_len)
            compressed.extend(curr_bytes[i : i + lit_len])
            i += lit_len
                
        compressed.append(0x00) # EOF
        frame_size = len(compressed)
        
        if total_compressed_size + frame_size > max_rom_bytes:
            print(f"⚠️ ROM limit reached at frame {len(all_frames) + skipped_frames} / {total_video_frames}.")
            break
            
        all_frames.append(compressed)
        total_compressed_size += frame_size
        prev_bytes = curr_bytes
        
    cap.release()
    
    print("Generating .h file...")
    with open(output_file, "w") as f:
        f.write("#pragma once\n")
        f.write("#include <stdint.h>\n\n")
        f.write(f"const uint16_t VIDEO_WIDTH = {width};\n")
        f.write(f"const uint16_t VIDEO_HEIGHT = {height};\n")
        f.write(f"const uint32_t VIDEO_FRAMES = {len(all_frames)};\n")
        f.write(f"const uint8_t VIDEO_FPS = {fps_target};\n")
        f.write(f"const bool VIDEO_GREY = {str(simulate_gray).lower()};\n\n")

        f.write("__attribute__((section(\".rodata\"))) const uint8_t video_data[] = {\n")
        for frame in all_frames:
            hex_strings = [f"0x{b:02X}" for b in frame]
            for j in range(0, len(hex_strings), 16):
                f.write("    " + ", ".join(hex_strings[j:j+16]) + ",\n")
        f.write("};\n")
        
    print(f"Done! Encoded frames: {len(all_frames)} (Skipped {skipped_frames}).")
    print(f"Generated array size: {total_compressed_size / 1024:.2f} KB ({(total_compressed_size / max_rom_bytes)*100:.1f}% of max allocation).")






if __name__ == "__main__":

    """
    compress_video_to_header("./video/bad_apple.mp4"
                                , output_file="../video/video_data.h"
                                , width=128, height=296
                                , max_rom_bytes=1.9 * 1024 * 1024
                                , horizontal_squeeze=1
                                , rotate_90=True
                                , simulate_gray=False
                            )
    """

    """
    compress_video_to_header("./video/bad_apple.mp4"
                                , output_file="../video/video_data.h"
                                , width=128, height=296
                                , max_rom_bytes=1.9 * 1024 * 1024
                                , horizontal_squeeze=0.8
                                , rotate_90=False
                                , simulate_gray=False
                            )
    """

    """  
    """
    compress_video_to_header("./video/Lamu.mp4"
                                , output_file="../video/video_data.h"
                                , width=128, height=296
                                , max_rom_bytes=1.9 * 1024 * 1024
                                , horizontal_squeeze=1
                                , rotate_90=True
                                , simulate_gray=False
                                , scale_factor = 0.90
                            )
    """
    """

    """
    compress_video_to_header("./video/Lamu.mp4"
                                , output_file="../video/video_data.h"
                                , width=128, height=296
                                , max_rom_bytes=1.9 * 1024 * 1024
                                , horizontal_squeeze=1
                                , rotate_90=True
                                , simulate_gray=True
                                , dark_thresh=80 
                                , light_thresh=170 
                                , scale_factor = 0.90
                            )
    """

    """
    compress_video_to_header("./video/ZUTOMAYO.mp4"
                                , output_file="../video/video_data.h"
                                , width=128, height=296
                                , max_rom_bytes=1.9 * 1024 * 1024
                                , horizontal_squeeze=1
                                , rotate_90=True
                                , simulate_gray=False
                            )
    """

    """    
    compress_video_to_header("./video/ZUTOMAYO.mp4"
                                , output_file="../video/video_data.h"
                                , width=128, height=296
                                , max_rom_bytes=1.9 * 1024 * 1024
                                , horizontal_squeeze=1
                                , rotate_90=True
                                , simulate_gray=True
                                , dark_thresh=80 
                                , light_thresh=170 
                            )
    """


