#ifndef RTSP_INPUT_H
#define RTSP_INPUT_H

#include <string>
#include <memory>
#include "input_base.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

class RTSPInput : public InputBase {
public:
    RTSPInput(const std::string& rtsp_url, std::shared_ptr<RistOutput> output);
    ~RTSPInput();
    
    // Virtual functions from InputBase
    bool start() override;
    void process() override;
    void stop() override;
    
private:
    // Initialize FFmpeg
    bool init_ffmpeg();
    
    // Open RTSP stream
    bool open_rtsp_stream();
    
    // Read packet from RTSP stream
    bool read_packet();
    
    std::string m_rtsp_url;
    bool m_running = false;
    
    // FFmpeg structures
    AVFormatContext* m_format_ctx = nullptr;
    AVPacket* m_packet = nullptr;
    int m_video_stream_idx = -1;
};

#endif // RTSP_INPUT_H