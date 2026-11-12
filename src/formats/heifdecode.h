#ifndef HEIFDECODE_H
#define HEIFDECODE_H

#include <libheif/heif.h>

// Decodes to interleaved RGBA. Where the build has it (MPV_FFMPEG_HEVC), FFmpeg's HEVC
// decoder is asked for explicitly: libheif ranks libde265 higher, yet FFmpeg is about
// 1.5x faster on Apple Silicon. A file FFmpeg rejects falls back to the default decoder.
inline heif_error heifDecodeRGBA(heif_image_handle* handle, heif_image** out)
{
#ifdef MPV_FFMPEG_HEVC
    heif_decoding_options* options = heif_decoding_options_alloc();
    options->decoder_id            = "ffmpeg";
    const heif_error err           = heif_decode_image(handle, out, heif_colorspace_RGB, heif_chroma_interleaved_RGBA, options);
    heif_decoding_options_free(options);
    if (err.code == heif_error_Ok)
    {
        return err;
    }
#endif
    return heif_decode_image(handle, out, heif_colorspace_RGB, heif_chroma_interleaved_RGBA, nullptr);
}

#endif   // HEIFDECODE_H
