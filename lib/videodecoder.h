#pragma once
#include <stdint.h>

class VideoDecoder {
    private:
        const uint8_t* video_data;
        uint32_t data_size;
        
        uint16_t width;
        uint16_t height;
        uint32_t total_frames;
        uint8_t fps;
        
        uint32_t stream_ptr;
        uint32_t current_frame;
        
        uint8_t* frame_buffer;
        uint32_t buffer_size;

    public:
        VideoDecoder(const uint8_t* data, uint32_t size, uint16_t w, uint16_t h, uint32_t frames, uint8_t video_fps);

        ~VideoDecoder();

        void reset();

        uint8_t* get_next_frame();

        // --- Getters ---
        uint16_t get_width() const { return width; }
        uint16_t get_height() const { return height; }
        uint8_t get_fps() const { return fps; }
        uint32_t get_buffer_size() const { return buffer_size; }
        bool is_finished() const { return current_frame >= total_frames; }
};