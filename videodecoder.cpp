/*
                std::ofstream ofile("video.mp4", std::ios::binary);
                offset = data.size() - imageEXIF.MicroVideoOffset;
                ofile.write((char*)(data.data()+ offset), data.size() - offset);
                ofile.close();
                int offset = data.size() - imageEXIF.MicroVideoOffset;
                QByteArray databuf = QByteArray(reinterpret_cast<char*>(data.data()+offset), data.size() - offset);
                QBuffer mediaStream(&databuf);

                QMediaPlayer* player = new QMediaPlayer;
                player->setMedia(QMediaContent(), &mediaStream);

                QVideoWidget* videoWidget = new QVideoWidget;
                player->setVideoOutput(videoWidget);
                videoWidget->show();

                player->play();
*/

/*
        QWidget *widget = new QWidget(this);
        widget->resize(400, 300);
        QVBoxLayout *layout = new QVBoxLayout;
        QMediaPlayer *player = new QMediaPlayer;
        QVideoWidget *vw = new QVideoWidget;
        layout->addWidget(vw);
        widget->setLayout(layout);
        player->setVideoOutput(vw);
//            player->setMedia(QUrl::fromLocalFile(R"(d:\media\films\George_Carlin_-_You_are_all_diseased_RUS.avi)"));
        player->setMedia(QUrl::fromLocalFile(R"(d:\_pictures\DCIMS10\2020-03-22\20200322_094753.mp4)"));
        player->setVolume(50);
        player->play();
        widget->show();
*/

// life could be better, but unfortunatelly Qt doen't want to use any codec
// that not registered in your system
// so we need to decode video manually.


#include "videodecoder.h"
#include "jpeginmemoryvideo.h"

#include <thread>
#include <chrono>
#include <mutex>
#include <QDebug>

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

void VideoDecoder::startAudioAndVideoDecoding()
{
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
            size_t sleepMs = m_videoMetaData.frameDuration * 1000 / 2;
            getFrame(m_queuePrefferedSize);
            while (thread_alive)
            {
                size_t queSize = 0;
                {
                    std::lock_guard<std::mutex> lock(m);
                    queSize = m_videoFramesQueue.size();
                    if (queSize && m_videoFramesQueue.back() == nullptr)
                    {
                        break;
                    }
                }
                //            qDebug()<<"queue size "<< queSize;
                if (queSize < m_queuePrefferedSize)
                {
                    getFrame(1);
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
            }

        })
        , [](std::thread* p) {p->join(); delete p; }
        );
}

std::shared_ptr<uint8_t> VideoDecoder::popVideoFrame()
{
    if (m_videoFramesQueue.size())
    {
        std::lock_guard<std::mutex> lock(m);
        auto el = m_videoFramesQueue.front();
        m_videoFramesQueue.pop();
        return el;
    }
    else
    {
        return nullptr;
    }
}

void VideoDecoder::initVideo()
{
    m_failedToLoad = true;

    unsigned char* fileStreamBuffer = (unsigned char*)av_malloc(FILESTREAMBUFFERSZ);
    if (fileStreamBuffer == nullptr) {
        // out of memory
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
        if (m_videoCodec == nullptr)
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
        return;
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

void VideoDecoder::getFrame(size_t frameCount)
{
    size_t frameIndex = 0;

    int frameFinished = 0;
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

            if (frameFinished != AVERROR_EOF)
            {
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
            }
            else
            {
                std::lock_guard<std::mutex> lock(m);
                m_videoFramesQueue.push(nullptr);
            }
            av_packet_unref(&packet);
            if (frameIndex >= frameCount)
            {
                break;
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
    }
}


//void VideoDecoder::decodeVideoFrames(int framesCount)
//{
//    //    std::vector<QImage*> images_vec;




//    // A IStream - you choose where it comes from
//    //IStream* fileStreamData;

//    // Alloc a buffer for the stream
//    unsigned char* fileStreamBuffer = (unsigned char*)av_malloc(FILESTREAMBUFFERSZ);
//    if (fileStreamBuffer == nullptr)
//    {
//        // out of memory
//    }
//    m_fileStreamBuffer.reset((unsigned char*)av_malloc(FILESTREAMBUFFERSZ), [](unsigned char* p) { av_free(p); });

//    // Get a AVContext stream
//    AVIOContext* ioContext = avio_alloc_context(
//                fileStreamBuffer,    // Buffer
//                FILESTREAMBUFFERSZ,  // Buffer size
//                0,                   // Buffer is only readable - set to 1 for read/write
//                &memVid,      // User (your) specified data
//                JpegUtils::FileStreamRead,      // Function - Reading Packets (see example)
//                0,                   // Function - Write Packets
//                JpegUtils::FileStreamSeek       // Function - Seek to position in stream (see example)
//                );
//    if (ioContext == nullptr)
//    {
//        qDebug() << "out of memory";
//        return images_vec;
//    }

//    // Allocate a AVContext
//    AVFormatContext* formatContext = avformat_alloc_context();

//    // Set up the Format Context
//    formatContext->pb = ioContext;
//    formatContext->flags |= AVFMT_FLAG_CUSTOM_IO; // we set up our own IO

//    // Open "file" (open our custom IO)
//    // Empty string is where filename would go. Doesn't matter since we aren't reading a file
//    // NULL params are format and demuxer settings, respectively
//    //        AVFormatContext* fmt_ctx = NULL;
//    //        if ( avformat_open_input(&fmt_ctx, R"(D:\Workspace\VS2017\MobilePhotoViewer\MobilePhotoViewerCore\video.mp4)", NULL, NULL))
//    if (avformat_open_input(&formatContext, "", nullptr, nullptr) < 0)
//    {
//        qDebug() << "error reading";
//        return images_vec;
//    }
//    else
//    {
//        AVDictionaryEntry* tag = NULL;
//        while ((tag = av_dict_get(formatContext->metadata, "", tag, AV_DICT_IGNORE_SUFFIX)))
//        {
//            printf("%s=%s\n", tag->key, tag->value);
//        }
//        av_dump_format(formatContext, 0, "", 0);
//        /////////////////////////////////////
//        int videoStreamIndex =-1;
//        int audioStreamIndex =-1;

//        AVCodecParameters* videoCodecContext = nullptr;
//        AVCodecParameters* audioCodecContext = nullptr;
//        AVCodec* videoCodec = nullptr;
//        for(int i=0; i<(int)formatContext->nb_streams; i++)
//        {
//            AVCodecContext* codecCtx = formatContext->streams[i]->codecpar;
//            if((codecCtx->codec_type==AVMEDIA_TYPE_VIDEO)
//                    && (videoStreamIndex < 0))
//            {
//                videoStreamIndex=i;
//                videoCodecContext = codecCtx;
//            }

//            else if((codecCtx->codec_type==AVMEDIA_TYPE_AUDIO)
//                    && (audioStreamIndex < 0))
//            {
//                audioStreamIndex=i;
//                audioCodecContext = codecCtx;
//            }
//        }
//        if (videoCodecContext)
//        {
//            videoCodec = avcodec_find_decoder(videoCodecContext->codec_id);
//            if (videoCodec == nullptr)
//            {
//                qDebug() << "video codec not found";
//            }
//        }
//        int errorCode = avcodec_open2(videoCodecContext, videoCodec, nullptr);
//        if (errorCode != 0)
//        {
//            qDebug() << "could not open codec";
//        }
//        size_t queueAheadCount = 32;
//        std::vector<AVFrame*> RgbaFrames;
//        uint32_t bytesPerFrame = av_image_get_buffer_size(AV_PIX_FMT_RGBA, videoCodecContext->width, videoCodecContext->height, 16);
//        RgbaFrames.resize(queueAheadCount);
//        for (uint32_t frame = 0; frame < queueAheadCount; ++frame)
//        {
//            RgbaFrames[frame] = av_frame_alloc();
//            RgbaFrames[frame]->linesize[0] = videoCodecContext->width * 4; // RGBA
//            RgbaFrames[frame]->data[0] = static_cast<uint8_t*>(av_malloc(bytesPerFrame));
//        }

//        SwsContext * swsContext = nullptr;
//        if (videoCodecContext->pix_fmt != AV_PIX_FMT_NONE)
//        {
//            swsContext = sws_getContext(videoCodecContext->width, videoCodecContext->height, videoCodecContext->pix_fmt,
//                                        videoCodecContext->width, videoCodecContext->height, AV_PIX_FMT_RGBA,
//                                        SWS_BILINEAR, nullptr, nullptr, nullptr);
//        }

//        int64_t start_time = formatContext->streams[videoStreamIndex]->start_time;
//        //        qDebug() << "start_time " << start_time;

//        int error = avformat_seek_file(formatContext, videoStreamIndex, start_time, start_time, start_time, 0);
//        //If seeking fails, try seeking JUST with AVSEEK_FLAG_ANY
//        if (error < 0)
//        {
//            error = av_seek_frame(formatContext, videoStreamIndex, start_time, AVSEEK_FLAG_ANY);
//        }
//        avcodec_flush_buffers(videoCodecContext);

//        AVFrame* tempFrame = av_frame_alloc();
//        size_t frameIndex = 0;
//        if (formatContext)
//        {
//            int frameFinished = 0;
//            AVPacket packet;
//            int returnCode = 0;

//            //If av_read_frame returns anything less than 0 it could either be an error, or the video could have stopped
//            while ((returnCode = av_read_frame(formatContext, &packet)) >= 0)
//            {
//                AVFrame* destFrame = RgbaFrames[frameIndex];

//                // Is this the video stream we found earlier?
//                if (packet.stream_index == videoStreamIndex)
//                {
//                    int decodeError = avcodec_send_packet(videoCodecContext ,&packet);
//                    if (decodeError != 0)
//                    {
//                        av_packet_unref(&packet);
//                        continue;
//                    }
//                    decodeError = avcodec_receive_frame(videoCodecContext, tempFrame);
//                    if (decodeError != 0)
//                    {
//                        av_packet_unref(&packet);
//                        continue;
//                    }

//                    if (frameFinished != AVERROR_EOF)
//                    {
//                        if (!swsContext)
//                        {
//                            swsContext = sws_getContext(videoCodecContext->width, videoCodecContext->height, videoCodecContext->pix_fmt,
//                                                        videoCodecContext->width, videoCodecContext->height, AV_PIX_FMT_RGBA,
//                                                        SWS_BILINEAR, nullptr, nullptr, nullptr);
//                        }
//                        // Convert the image to RGBA.
//                        sws_scale(swsContext, tempFrame->data, tempFrame->linesize, 0, videoCodecContext->height, destFrame->data, destFrame->linesize);

//                        //Some formats will automatically parse the presentation time stamp from the packet into the frame
//                        //Some don't, so we do it manually just in case
//                        destFrame->pts = packet.pts;

//                        //                            av_packet_unref(&packet);
//                        //                            break;
//                    }
//                }

//                av_packet_unref(&packet);
//                ++frameIndex;
//                if (frameIndex >= queueAheadCount)
//                {
//                    break;
//                }
//            }
//            images_vec.resize(queueAheadCount);
//            int videoWidth = videoCodecContext->width;
//            int videoHeight = videoCodecContext->height;

//            int linesize[AV_NUM_DATA_POINTERS] = { 0 };

//            linesize[0] = videoWidth * 4;

//            for (size_t i = 0; i < queueAheadCount; ++i)
//            {
//                uint8_t * buf = new uint8_t[videoWidth * videoHeight * 4];
//                images_vec[i] = new QImage(buf, videoWidth, videoHeight, QImage::Format_ARGB32);
//                memcpy(buf, RgbaFrames[i]->data[0], videoWidth * videoHeight * 4);

//                //                uint8_t *data[AV_NUM_DATA_POINTERS] = { 0 };
//                //                data[0] = (uint8_t *)buf;
//                //                int h = sws_scale(swsContext, RgbaFrames[i]->data, RgbaFrames[i]->linesize, 0, videoCodecContext->height, data, linesize);
//                //                if (h <0)
//                //                {
//                //                    qDebug() << "wrong decoding";
//                //                }
//            }

//            for (size_t frame = 0; frame < RgbaFrames.size(); ++frame)
//            {
//                if (RgbaFrames[frame])
//                {
//                    av_free(RgbaFrames[frame]->data[0]);
//                    av_frame_free(&RgbaFrames[frame]);

//                    RgbaFrames[frame] = nullptr;
//                }
//            }

//            if (tempFrame != nullptr)
//            {
//                av_frame_free(&tempFrame);
//            }
//            /*
//            AVPacket packet;
//            int err = av_read_frame(formatContext, &packet);
//            while(err>=0) {
//                // Is this a packet from the video stream?
//                if(packet.stream_index==videoStreamIndex)
//                {
//                    AVCodecContext* m_codecContext
//                }
//            }
//*/

//            ////////////////////////////////////
//        }

//        // Do something with the formatContext

//        // Free resources!
//        avformat_close_input(&formatContext);
//        av_free(ioContext);

//        return images_vec;
//    }

//}
