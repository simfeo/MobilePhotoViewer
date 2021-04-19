// life could be better, but unfortunatelly Qt doen't want to use any codec
// that not registered in your system
// so we need to decode video manually.


#include "videodecoder.h"
#include "jpeginmemoryvideo.h"

#include <thread>
#include <chrono>
#include <mutex>
#include <QDebug>

extern "C"
{
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
}

namespace
{
    const size_t MAX_VIDEOMEMORY_CACHE_SIZE = 100 * 1024 * 1024;
    const int FILESTREAMBUFFERSZ = 1024 * 16;
    bool thread_alive = true;
    std::mutex m;
}

VideoDecoder::VideoDecoder(std::shared_ptr<JpegUtils::InMemoryVid> memoryVideo) :
    m_failedToLoad(false)
    , m_audioStreamIndex(-1)
    , m_videoStreamIndex(-1)
    , m_queuePrefferedSize(5)
    , m_swsVideoContext(nullptr)
    , m_swsAudioContext(nullptr)
    , m_memoryVideo(memoryVideo)
    , m_ioContext(nullptr)
    , m_videoCodec(nullptr)
    , m_audioCodec(nullptr)
    , m_videoCodecContext(nullptr)
    , m_audioCodecContext(nullptr)
    , m_formatContext(nullptr)
    , m_tempVideoFrame(nullptr)
    , m_tempAudioFrame(nullptr)
    , m_videoThread(nullptr)
{
    initVideo();
}

VideoDecoder::~VideoDecoder()
{
    if (m_videoThread)
    {
        thread_alive = false;
        m_videoThread.reset();
    }
    deInitVideo();
}

const VideoMetaData& VideoDecoder::getMetadataInfo() const
{
    return m_videoMetaData;
}

bool VideoDecoder::isVideoInited() const
{
    return !m_failedToLoad;
}

bool VideoDecoder::hasAudioSource() const
{
    return m_pcmFrames.size();
}

void VideoDecoder::startAudioAndVideoDecoding()
{
    thread_alive = false;
    m_videoThread.reset();
    thread_alive = true;
    if (m_pcmFrames.empty())
    {
        initAudioBuffers();
    }

    int64_t start_time = m_formatContext->streams[m_videoStreamIndex]->start_time;
    int error = avformat_seek_file(m_formatContext.get(), m_videoStreamIndex, start_time, start_time, start_time, 0);
    //If seeking fails, try seeking JUST with AVSEEK_FLAG_ANY
    if (error < 0)
    {
        error = av_seek_frame(m_formatContext.get(), m_videoStreamIndex, start_time, AVSEEK_FLAG_ANY);
    }

    avcodec_flush_buffers(m_videoCodecContext.get());

    m_tempVideoFrame.reset(av_frame_alloc(), [](AVFrame* p) {av_frame_free(&p); });

    m_queuePrefferedSize = MAX_VIDEOMEMORY_CACHE_SIZE / m_videoMetaData.uncompressedBufferSize;
    m_queuePrefferedSize = m_queuePrefferedSize < 5 ? 5 : m_queuePrefferedSize;
    if (m_videoFramesQueue.size())
    {
        qDebug() << "starting with no empty queue";
    }

    m_videoThread.reset(new std::thread([this]()
        {
            size_t sleepMs = m_videoMetaData.frameDuration * 100;
            getFrame(m_queuePrefferedSize);
            while (thread_alive)
            {
                size_t queSize = 0;
                {
                    std::lock_guard<std::mutex> lock(m);
                    queSize = m_videoFramesQueue.size();
                    if (queSize && m_videoFramesQueue.back() == nullptr)
                    {
                        thread_alive = false;
                        break;
                    }
                }
                //            qDebug()<<"queue size "<< queSize;
                if (queSize < m_queuePrefferedSize)
                {
                    if (!getFrame(1))
                    {
                        thread_alive = false;
                        break;
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
            }

        })
        , [](std::thread* p) {p->join(); delete p; }
        );
}

std::shared_ptr<uint8_t> VideoDecoder::popVideoFrame(bool& isThreadAlive)
{
    if (m_videoFramesQueue.size())
    {
        std::lock_guard<std::mutex> lock(m);
        auto el = m_videoFramesQueue.front();
        m_videoFramesQueue.pop();
        isThreadAlive = thread_alive;
        return el;
    }
    else
    {
        isThreadAlive = thread_alive;
        return nullptr;
    }
}

std::vector<PcmFrame>& VideoDecoder::getPcmFrames()
{
    return m_pcmFrames;
}

void VideoDecoder::initVideo()
{
    m_failedToLoad = true;

    unsigned char* fileStreamBuffer = (unsigned char*)av_malloc(FILESTREAMBUFFERSZ);
    if (fileStreamBuffer == nullptr)
    {
        return;
    }

    // Get a AVContext stream
    m_ioContext.reset(avio_alloc_context(
        fileStreamBuffer,     // Buffer
        FILESTREAMBUFFERSZ,           // Buffer size
        0,                            // Buffer is only readable - set to 1 for read/write
        m_memoryVideo.get(),          // User (your) specified data
        JpegUtils::FileStreamRead,    // Function - Reading Packets (see example)
        0,                            // Function - Write Packets
        JpegUtils::FileStreamSeek     // Function - Seek to position in stream (see example)
    ),
        [](AVIOContext* p) { av_free(p); });
    if (m_ioContext.get() == nullptr)
    {
        qDebug() << "out of memory";
        return;
    }

    // Allocate a AVContext
    m_formatContext.reset(avformat_alloc_context(), [](AVFormatContext* p) { avformat_close_input(&p); });

    // Set up the Format Context
    m_formatContext->pb = m_ioContext.get();
    m_formatContext->flags |= AVFMT_FLAG_CUSTOM_IO; // we set up our own IO

    // Open "file" (open our custom IO)
    // Empty string is where filename would go. Doesn't matter since we aren't reading a file
    // NULL params are format and demuxer settings, respectively
    //        AVFormatContext* fmt_ctx = NULL;
    //        if ( avformat_open_input(&fmt_ctx, R"(D:\Workspace\VS2017\MobilePhotoViewer\MobilePhotoViewerCore\video.mp4)", NULL, NULL))

    AVFormatContext* fmt_ctx = m_formatContext.get();
    if (avformat_open_input(&fmt_ctx, "", nullptr, nullptr) < 0)
    {
        qDebug() << "error reading";
        return;
    }

    AVDictionaryEntry* tag = NULL;
    while ((tag = av_dict_get(m_formatContext.get()->metadata, "", tag, AV_DICT_IGNORE_SUFFIX)))
    {
        qDebug() << tag->key << tag->value;
    }

    for (int i = 0; i < (int)m_formatContext->nb_streams; i++)
    {
        AVCodecParameters* codecCtx = m_formatContext->streams[i]->codecpar;
        if ((codecCtx->codec_type == AVMEDIA_TYPE_VIDEO)
            && (m_videoStreamIndex < 0))
        {
            m_videoStreamIndex = i;
            AVCodecContext* ctx = avcodec_alloc_context3(NULL);
            if (ctx == nullptr)
            {
                qDebug() << "insufficient av memory oO";
            }
            if (int ret = avcodec_parameters_to_context(ctx, codecCtx))
            {
                qDebug() << "failed to find codec return code:" << ret;
            }
            m_videoCodecContext.reset(ctx, [](AVCodecContext* p) { avcodec_close(p); });
        }

        else if ((codecCtx->codec_type == AVMEDIA_TYPE_AUDIO)
            && (m_audioStreamIndex < 0))
        {
            m_audioStreamIndex = i;
            AVCodecContext* ctx = avcodec_alloc_context3(NULL);
            if (ctx == nullptr)
            {
                qDebug() << "insufficient av memory oO";
            }
            if (int ret = avcodec_parameters_to_context(ctx, codecCtx))
            {
                qDebug() << "failed to find codec return code:" << ret;
            }
            m_audioCodecContext.reset(ctx, [](AVCodecContext* p) { avcodec_close(p); });
        }
    }

    if (m_videoCodecContext.get())
    {
        m_videoCodec.reset(avcodec_find_decoder(m_videoCodecContext->codec_id), [](AVCodec* p) { p = nullptr; });
        if (m_videoCodec == nullptr)
        {
            qDebug() << "video codec not found";
            return;
        }
        int errorCode = avcodec_open2(m_videoCodecContext.get(), m_videoCodec.get(), nullptr);
        if (errorCode != 0)
        {
            qDebug() << "could not open video codec";
            return;
        }
    }

    if (m_audioCodecContext.get())
    {
        m_audioCodec.reset(avcodec_find_decoder(m_audioCodecContext->codec_id), [](AVCodec* p) { p = nullptr; });
        if (m_audioCodec == nullptr)
        {
            qDebug() << "audio codec not found";
            return;
        }
        int errorCode = avcodec_open2(m_audioCodecContext.get(), m_audioCodec.get(), nullptr);
        if (errorCode != 0)
        {
            qDebug() << "could not open audio codec";
            return;
        }
    }
#ifdef _DEBUG
    av_dump_format(m_formatContext.get(), 0, "", 0);
#endif
    if (m_videoCodecContext->pix_fmt != AV_PIX_FMT_NONE)
    {
        m_swsVideoContext = sws_getContext(m_videoCodecContext->width, m_videoCodecContext->height, m_videoCodecContext->pix_fmt,
            m_videoCodecContext->width, m_videoCodecContext->height, AV_PIX_FMT_RGBA,
            SWS_BILINEAR, nullptr, nullptr, nullptr);
    }

    initVideoMeta();

    int64_t start_time = m_formatContext->streams[m_videoStreamIndex]->start_time;

    //        qDebug() << "start_time " << start_time;

    int error = avformat_seek_file(m_formatContext.get(), m_videoStreamIndex, start_time, start_time, start_time, 0);
    //If seeking fails, try seeking JUST with AVSEEK_FLAG_ANY
    if (error < 0)
    {
        error = av_seek_frame(m_formatContext.get(), m_videoStreamIndex, start_time, AVSEEK_FLAG_ANY);
        if (error < 0)
        {
            return;
        }
    }
    m_failedToLoad = false;
}

void VideoDecoder::initVideoMeta()
{
    m_videoMetaData.width = m_videoCodecContext->width;
    m_videoMetaData.height = m_videoCodecContext->height;
    m_videoMetaData.rotation = 0;

    AVDictionary* streamMeta = m_formatContext->streams[m_videoStreamIndex]->metadata;
    if (streamMeta && !(av_dict_count(streamMeta) == 1 && av_dict_get(streamMeta, "language", NULL, 0)))
    {
        const AVDictionaryEntry* tag = NULL;
        while ((tag = av_dict_get(streamMeta, "", tag, AV_DICT_IGNORE_SUFFIX)))
        {
            if (strcmp("rotate", tag->key) == 0)
            {
                m_videoMetaData.rotation = atoi(tag->value);
                break;
            }
        }
    }
    m_videoMetaData.frameDuration = 1.0f
        * m_formatContext->streams[m_videoStreamIndex]->avg_frame_rate.den
        / m_formatContext->streams[m_videoStreamIndex]->avg_frame_rate.num;
    m_videoMetaData.uncompressedBufferSize = av_image_get_buffer_size(AV_PIX_FMT_RGBA, m_videoCodecContext->width, m_videoCodecContext->height, 16);
}

void VideoDecoder::deInitVideo()
{
    m_videoCodecContext.reset();
    m_audioCodecContext.reset();
    m_formatContext.reset();
    m_ioContext.reset();
}

bool VideoDecoder::getFrame(size_t frameCount)
{
    //std::this_thread::sleep_for(std::chrono::milliseconds(50));
    size_t frameIndex = 0;
    AVPacket packet;

    std::shared_ptr<AVFrame> videoFrame(av_frame_alloc(), [](AVFrame* p) {
        if (p->data[0])
        {
            av_free(p->data[0]);
        }
        av_frame_free(&p);
        });

    videoFrame->linesize[0] = m_videoCodecContext->width * 4; // RGBA
    videoFrame->data[0] = static_cast<uint8_t*>(av_malloc(m_videoMetaData.uncompressedBufferSize));

    int returnCode = 0;
    //If av_read_frame returns anything less than 0 it could either be an error, or the video could have stopped
    while ((returnCode = av_read_frame(m_formatContext.get(), &packet)) >= 0)
    {
        // Is this the video stream we found earlier?
        if (packet.stream_index == m_videoStreamIndex)
        {
            int decodeError = avcodec_send_packet(m_videoCodecContext.get(), &packet);
            if (decodeError != 0)
            {
                av_packet_unref(&packet);
                continue;
            }
            decodeError = avcodec_receive_frame(m_videoCodecContext.get(), m_tempVideoFrame.get());
            if (decodeError != 0)
            {
                av_packet_unref(&packet);
                continue;
            }


            if (!m_swsVideoContext)
            {
                m_swsVideoContext = sws_getContext(m_videoCodecContext->width, m_videoCodecContext->height, m_videoCodecContext->pix_fmt,
                    m_videoCodecContext->width, m_videoCodecContext->height, AV_PIX_FMT_RGBA,
                    SWS_BILINEAR, nullptr, nullptr, nullptr);
            }
            // Convert the image to RGBA.
            sws_scale(m_swsVideoContext, m_tempVideoFrame->data, m_tempVideoFrame->linesize, 0, m_videoCodecContext->height, videoFrame->data, videoFrame->linesize);

            //Some formats will automatically parse the presentation time stamp from the packet into the frame
            //Some don't, so we do it manually just in case
            videoFrame->pts = packet.pts;
            std::shared_ptr<uint8_t> buf(new uint8_t[m_videoCodecContext->width * m_videoCodecContext->height * 4], [](uint8_t* p) { delete[] p; });
            memcpy(buf.get(), videoFrame->data[0], m_videoCodecContext->width * m_videoCodecContext->height * 4);
            {
                std::lock_guard<std::mutex> lock(m);
                m_videoFramesQueue.push(buf);
            }
            av_frame_unref(m_tempVideoFrame.get());

            av_packet_unref(&packet);
            if (frameIndex >= frameCount)
            {
                return true;
            }
            ++frameIndex;
        }
        else
        {
            av_packet_unref(&packet);
        }
    }
    if (returnCode < 0)
    {
        std::lock_guard<std::mutex> lock(m);
        m_videoFramesQueue.push(nullptr);
        return false;
    }
    return false;
}

void VideoDecoder::initAudioBuffers()
{
    if (m_audioStreamIndex < 0 || !m_audioCodecContext || !m_audioCodec)
    {
        return;
    }

    int start_time = m_formatContext->streams[m_audioStreamIndex]->start_time;
    size_t frame_count = m_formatContext->streams[m_audioStreamIndex]->nb_frames;

    avcodec_flush_buffers(m_audioCodecContext.get());

    int error = avformat_seek_file(m_formatContext.get(), m_audioStreamIndex, start_time, start_time, start_time, 0);
    //If seeking fails, try seeking JUST with AVSEEK_FLAG_ANY
    if (error < 0)
    {
        error = av_seek_frame(m_formatContext.get(), m_audioStreamIndex, start_time, AVSEEK_FLAG_ANY);
        if (error < 0)
        {
            return;
        }
    }
    size_t frameIndex = 0;
    AVPacket packet;

    m_tempAudioFrame.reset(av_frame_alloc(), [](AVFrame* p) {av_frame_free(&p); });

    std::shared_ptr<AVFrame> audioFrame(av_frame_alloc(), [](AVFrame* p) {
        if (p->data[0])
        {
            av_free(p->data[0]);
        }
        av_frame_free(&p);
        });


    std::shared_ptr<SwrContext> swr_ctx(nullptr);

    int returnCode = 0;
    //If av_read_frame returns anything less than 0 it could either be an error, or the video could have stopped
    while ((returnCode = av_read_frame(m_formatContext.get(), &packet)) >= 0)
    {
        // Is this the video stream we found earlier?
        if (packet.stream_index == m_audioStreamIndex)
        {
            int decodeError = avcodec_send_packet(m_audioCodecContext.get(), &packet);
            if (decodeError != 0)
            {
                av_packet_unref(&packet);
                continue;
            }
            decodeError = avcodec_receive_frame(m_audioCodecContext.get(), m_tempAudioFrame.get());
            if (decodeError != 0)
            {
                av_packet_unref(&packet);
                continue;
            }

            if (!swr_ctx)
            {
                swr_ctx.reset(swr_alloc(), [](SwrContext* p) {swr_free(&p); });
                swr_alloc_set_opts(swr_ctx.get(), m_audioCodecContext->channel_layout,
                    AV_SAMPLE_FMT_S16,
                    48000,
                    m_audioCodecContext->channels,
                    m_audioCodecContext->sample_fmt,
                    m_audioCodecContext->sample_rate,
                    0, 0
                );
                swr_init(swr_ctx.get());
            }

            size_t outsize = av_samples_get_buffer_size(NULL, m_audioCodecContext->channels,
                m_tempAudioFrame->nb_samples,
                AV_SAMPLE_FMT_S16,
                0);

            PcmFrame pc;
            pc.buffer.resize(outsize);
            uint8_t* data[1];
            data[0] = static_cast<uint8_t*>(&pc.buffer[0]);
            pc.start_time = 1000 * m_tempAudioFrame->pts / m_audioCodecContext->sample_rate;
            pc.duration = 1000 * m_tempAudioFrame->pkt_duration / m_audioCodecContext->sample_rate;

            int len = swr_convert(swr_ctx.get(), data, static_cast<int>(pc.buffer.size()),
                (const uint8_t**)m_tempAudioFrame->data,
                m_tempAudioFrame->nb_samples
            );
            if (len <= 0)
            {
                qDebug() << "no any audio sample converted converted";
            }
            m_pcmFrames.push_back(pc);

            av_packet_unref(&packet);
            ++frameIndex;
            if (frameIndex >= frame_count)
            {
                break;
            }
        }
        else
        {
            av_packet_unref(&packet);
        }
    }
}

