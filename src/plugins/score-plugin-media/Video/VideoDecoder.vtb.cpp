
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/pixdesc.h>
#include <libavcodec/videotoolbox.h>
#include <libavutil/hwcontext_videotoolbox.h>
}

namespace Video
{
#if 0
int videotoolbox_retrieve_data(AVCodecContext *s, AVFrame *frame) {

    CVPixelBufferRef pixbuf = (CVPixelBufferRef)frame->data[3];
    OSType pixel_format = CVPixelBufferGetPixelFormatType(pixbuf);
    CVReturn err;
    int planes, i;
    char codec_str[32];

    switch (pixel_format) {
        case kCVPixelFormatType_420YpCbCr8Planar: frame->format = AV_PIX_FMT_YUV420P; break;
        case kCVPixelFormatType_422YpCbCr8:       frame->format = AV_PIX_FMT_UYVY422; break;
        case kCVPixelFormatType_32BGRA:           frame->format = AV_PIX_FMT_BGRA; break;
#ifdef kCFCoreFoundationVersionNumber10_7
        case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange: frame->format = AV_PIX_FMT_NV12; break;
#endif
        default:
            // av_get_codec_tag_string(codec_str, sizeof(codec_str), s->codec_tag);
            av_log(NULL, AV_LOG_ERROR,
                   "%s: Unsupported pixel format\n", codec_str);
            return AVERROR(ENOSYS);
    }

    err = CVPixelBufferLockBaseAddress(pixbuf, kCVPixelBufferLock_ReadOnly);
    if (err != kCVReturnSuccess) {
        av_log(NULL, AV_LOG_ERROR, "Error locking the pixel buffer.\n");
        return AVERROR_UNKNOWN;
    }

    if (CVPixelBufferIsPlanar(pixbuf)) {
        planes = CVPixelBufferGetPlaneCount(pixbuf);
        for (i = 0; i < planes; i++) {
            frame->data[i]     = reinterpret_cast<uint8_t *>(CVPixelBufferGetBaseAddressOfPlane(pixbuf, i));
            frame->linesize[i] = CVPixelBufferGetBytesPerRowOfPlane(pixbuf, i);
        }
    } else {
        frame->data[0] = reinterpret_cast<uint8_t *>(CVPixelBufferGetBaseAddress(pixbuf));
        frame->linesize[0] = CVPixelBufferGetBytesPerRow(pixbuf);
    }

    CVPixelBufferUnlockBaseAddress(pixbuf, kCVPixelBufferLock_ReadOnly);

    return 0;
}
#endif

}
