#ifndef VIDEODECODER_H
#define VIDEODECODER_H

#include <QString>
#include <memory>
#include <queue>
#include <vector>

extern "C"
{
#include <libavformat/avformat.h>
#include <libavutil/dict.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

class QImage;

namespace JpegUtils
{
    class InMemoryVid;
}

namespace std
{
    class thread;
}

struct VideoMetaData
{
    float   frameDuration = 0.0f;
    int     rotation = 0;
    size_t  width = 0;
    size_t  height = 0;
    size_t  uncompressedBufferSize = 0;
};

struct PcmFrame
{
    std::vector<uint8_t>    buffer;
    uint64_t                start_time;
    uint32_t                duration;
};

class VideoDecoder
{
public:
    VideoDecoder(std::shared_ptr<JpegUtils::InMemoryVid> memoryVideo);
    ~VideoDecoder();

    // copy metadata of current video file
    // input: pointer of your own VideoMetaData struct
    const VideoMetaData& getMetadataInfo() const;
    // return pointer to buffer of ARGB32 image
    // buffer size stored in VideoMetaData.uncompressedBufferSize
    bool isVideoInited() const;
    bool hasAudioSource() const;
    void startAudioAndVideoDecoding();
    std::shared_ptr<uint8_t> popVideoFrame();
    std::vector<PcmFrame>& getPcmFrames();

private:
    void initVideo();
    void initVideoMeta();
    void deInitVideo();
    void getFrame(size_t frameCount);
    void initAudioBuffers();

private:
    bool                                    m_failedToLoad;
    int                                     m_audioStreamIndex;
    int                                     m_videoStreamIndex;
    size_t                                  m_queuePrefferedSize;
    size_t                                  m_currentVideoBufferFrameIndex;
    SwsContext*                             m_swsVideoContext;
    SwsContext*                             m_swsAudioContext;
    std::shared_ptr<JpegUtils::InMemoryVid> m_memoryVideo;
    VideoMetaData                           m_videoMetaData;
    std::queue<std::shared_ptr<uint8_t>>    m_videoFramesQueue;

    std::shared_ptr<AVIOContext>            m_ioContext;
    std::shared_ptr<AVCodec>                m_videoCodec;
    std::shared_ptr<AVCodec>                m_audioCodec;
    std::shared_ptr<AVCodecContext>         m_videoCodecContext;
    std::shared_ptr<AVCodecContext>         m_audioCodecContext;
    std::shared_ptr<AVFormatContext>        m_formatContext;
    std::shared_ptr<AVFrame>                m_tempVideoFrame;
    std::shared_ptr<AVFrame>                m_tempAudioFrame;
    std::shared_ptr<std::thread>            m_videoThread;
    std::vector<PcmFrame>                   m_pcmFrames;
};

#endif // VIDEODECODER_H
