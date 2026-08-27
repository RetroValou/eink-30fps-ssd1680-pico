#include "videodecoder.h"
#include <string.h>


// La liste d'initialisation se trouve dans l'implémentation
VideoDecoder::VideoDecoder(const uint8_t* data, uint32_t size, uint16_t w, uint16_t h, uint32_t frames, uint8_t video_fps)
    : video_data(data), data_size(size), width(w), height(h), total_frames(frames), fps(video_fps),
      stream_ptr(0), current_frame(0) 
{
    buffer_size = (width * height) / 8;
    frame_buffer = new uint8_t[buffer_size];
    memset(frame_buffer, 0, buffer_size);
}


VideoDecoder::~VideoDecoder() {
    delete[] frame_buffer;
}


void VideoDecoder::reset() {
    memset(frame_buffer, 0, buffer_size);
    stream_ptr = 0;
    current_frame = 0;
}


uint8_t* VideoDecoder::get_next_frame() {
    if (current_frame >= total_frames || stream_ptr >= data_size) {
        return nullptr;
    }
    
    uint32_t buf_ptr = 0;
    
    while (true) {
        uint8_t cmd = video_data[stream_ptr++];
        
        if (cmd == 0x00) {
            // 0x00 = Fin de la frame (EOF)
            break;
        }
        
        uint8_t type = cmd & 0xC0; // Isole les 2 premiers bits
        uint8_t count = cmd & 0x3F; // Isole les 6 bits suivants (longueur)
        
        if (type == 0x00) {
            // SKIP : Les pixels n'ont pas changé
            buf_ptr += count;
        } 
        else if (type == 0x40) {
            // LITERAL : Copie les 'count' octets mixtes
            for (int i = 0; i < count; i++) {
                frame_buffer[buf_ptr++] = video_data[stream_ptr++];
            }
        }
        else if (type == 0x80) {
            // FILL ZERO : Remplit 'count' octets en noir pur
            memset(&frame_buffer[buf_ptr], 0x00, count);
            buf_ptr += count;
        }
        else if (type == 0xC0) {
            // FILL ONE : Remplit 'count' octets en blanc pur
            memset(&frame_buffer[buf_ptr], 0xFF, count);
            buf_ptr += count;
        }
    }
    
    current_frame++;
    return frame_buffer;
}