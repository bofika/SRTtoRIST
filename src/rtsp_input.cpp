#include "rtsp_input.h"
#include <iostream>

RTSPInput::RTSPInput(const std::string& rtsp_url, std::shared_ptr<RistOutput> output)
    : m_rtsp_url(rtsp_url) {
    m_outputs.push_back(output);
}

RTSPInput::~RTSPInput() {
    stop();
}

bool RTSPInput::init_ffmpeg() {
    // Register all formats and codecs (deprecated in newer FFmpeg, but kept for compatibility)
#if LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(58, 9, 100)
    av_register_all();
#endif
    
    // Initialize network
    avformat_network_init();
    
    // Allocate packet
    m_packet = av_packet_alloc();
    if (!m_packet) {
        std::cerr << "Failed to allocate packet" << std::endl;
        return false;
    }
    
    return true;
}

bool RTSPInput::open_rtsp_stream() {
    // Set up format context
    m_format_ctx = avformat_alloc_context();
    if (!m_format_ctx) {
        std::cerr << "Failed to allocate format context" << std::endl;
        return false;
    }
    
    // Set options
    AVDictionary* options = nullptr;
    av_dict_set(&options, "rtsp_transport", "tcp", 0); // Use TCP for RTSP
    av_dict_set(&options, "stimeout", "5000000", 0);   // 5 second timeout
    
    // Open input
    int ret = avformat_open_input(&m_format_ctx, m_rtsp_url.c_str(), nullptr, &options);
    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errbuf, sizeof(errbuf));
        std::cerr << "Failed to open RTSP input: " << errbuf << std::endl;
        avformat_close_input(&m_format_ctx);
        av_dict_free(&options);
        return false;
    }
    
    // Free options dictionary
    av_dict_free(&options);
    
    // Get stream information
    ret = avformat_find_stream_info(m_format_ctx, nullptr);
    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errbuf, sizeof(errbuf));
        std::cerr << "Failed to find stream info: " << errbuf << std::endl;
        avformat_close_input(&m_format_ctx);
        return false;
    }
    
    // Find video stream
    m_video_stream_idx = -1;
    for (unsigned int i = 0; i < m_format_ctx->nb_streams; i++) {
        if (m_format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            m_video_stream_idx = i;
            break;
        }
    }
    
    if (m_video_stream_idx == -1) {
        std::cerr << "Failed to find video stream in RTSP source" << std::endl;
        avformat_close_input(&m_format_ctx);
        return false;
    }
    
    // Print stream info
    av_dump_format(m_format_ctx, 0, m_rtsp_url.c_str(), 0);
    
    std::cout << "RTSP stream opened successfully" << std::endl;
    return true;
}

bool RTSPInput::start() {
    if (!init_ffmpeg()) {
        return false;
    }
    
    if (!open_rtsp_stream()) {
        return false;
    }
    
    m_running = true;
    return true;
}

void RTSPInput::process() {
    if (!m_running) {
        return;
    }
    
    read_packet();
}

bool RTSPInput::read_packet() {
    int ret = av_read_frame(m_format_ctx, m_packet);
    if (ret < 0) {
        if (ret == AVERROR_EOF) {
            std::cout << "End of RTSP stream" << std::endl;
            m_running = false;
        } else if (ret == AVERROR(EAGAIN)) {
            // Resource temporarily unavailable, try again later
            return false;
        } else {
            char errbuf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, errbuf, sizeof(errbuf));
            std::cerr << "Error reading frame: " << errbuf << std::endl;
            
            // Attempt to reconnect for certain errors
            if (ret == AVERROR(EIO) || ret == AVERROR(ETIMEDOUT)) {
                std::cout << "Attempting to reconnect to RTSP stream..." << std::endl;
                stop();
                m_running = start();
            }
        }
        return false;
    }
    
    // Check if packet is from video stream
    if (m_packet->stream_index == m_video_stream_idx) {
        // Forward packet to RIST output
        for (auto& output : m_outputs) {
            output->send_data(reinterpret_cast<char*>(m_packet->data), m_packet->size);
        }
    }
    
    // Free packet
    av_packet_unref(m_packet);
    return true;
}

void RTSPInput::stop() {
    m_running = false;
    
    // Clean up FFmpeg resources
    if (m_packet) {
        av_packet_free(&m_packet);
        m_packet = nullptr;
    }
    
    if (m_format_ctx) {
        avformat_close_input(&m_format_ctx);
        m_format_ctx = nullptr;
    }
    
    m_video_stream_idx = -1;
}
