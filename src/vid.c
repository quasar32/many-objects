#include <glad/gl.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
#include "draw.h"
#include "misc.h"
#include "sim.h"

#define WIDTH 1280
#define HEIGHT 960 

#define FPS 24

static uint8_t pixels[WIDTH * HEIGHT * 3];

static void encode(AVCodecContext *cctx, AVFrame *frame, AVPacket *pkt, 
                   AVFormatContext *fmtctx, AVStream *vid) {
    int ret = avcodec_send_frame(cctx, frame);
    if (ret < 0)
        die("avcodec_send_frame: %s\n", av_err2str(ret));
    while (ret >= 0) {
        ret = avcodec_receive_packet(cctx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) 
            return;
        if (ret < 0)
            die("avcodec_receive_packet: %s\n", ret);
        av_packet_rescale_ts(pkt, cctx->time_base, vid->time_base);
        av_write_frame(fmtctx, pkt);
        av_packet_unref(pkt);
    }
}    

static void create_video(const char *path) {
    struct SwsContext *sws = sws_getContext(WIDTH, HEIGHT, AV_PIX_FMT_RGB24,
                                            WIDTH, HEIGHT, AV_PIX_FMT_YUV444P,
                                            0, NULL, NULL, NULL);
    if (!sws) {
        die("sws_getContext\n");
    }
    const uint8_t *src = pixels + WIDTH * 3 * (HEIGHT - 1);
    int src_stride = -WIDTH * 3;
    const AVOutputFormat *outfmt = av_guess_format(NULL, path, NULL);
    if (!outfmt) {
        die("av_guess_format\n");
    }
    AVFormatContext *fmtctx;
    int ret = avformat_alloc_output_context2(&fmtctx, outfmt, NULL, path);
    if (ret < 0) {
        die("avformat_free_context: %s\n", av_err2str(ret));
    }
    const AVCodec *codec = codec = avcodec_find_encoder(outfmt->video_codec);
    if (!codec) {
        die("avcodec_find_encoder\n");
    }
    AVStream *vid = avformat_new_stream(fmtctx, codec);
    if (!vid) {
        die("avformat_new_stream\n");
    }
    AVCodecContext *cctx = avcodec_alloc_context3(codec);
    if (!cctx) {
        die("avcodec_alloc_context3\n");
    }
    vid->codecpar->codec_id = outfmt->video_codec;
    vid->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    vid->codecpar->width = WIDTH;
    vid->codecpar->height = HEIGHT;
    vid->codecpar->format = AV_PIX_FMT_YUV444P;
    vid->codecpar->bit_rate = 4000000;
    av_opt_set(cctx, "preset", "ultrafast", 0);
    avcodec_parameters_to_context(cctx, vid->codecpar);
    cctx->time_base.num = 1; 
    cctx->time_base.den = FPS; 
    cctx->framerate.num = FPS; 
    cctx->framerate.den = 1; 
    avcodec_parameters_from_context(vid->codecpar, cctx);
    ret = avcodec_open2(cctx, codec, NULL);
    if (ret < 0) {
        die("avcodec_open2: %s\n", av_err2str(ret));
    }
    ret = avio_open(&fmtctx->pb, path, AVIO_FLAG_WRITE);
    if (ret < 0) {
        die("avio_open: %s\n", av_err2str(ret));
    }
    ret = avformat_write_header(fmtctx, NULL);
    if (ret < 0)
        die("avformat_write_header: %s\n", av_err2str(ret));
    AVPacket *pkt = av_packet_alloc();
    if (!pkt) {
        die("av_packet_alloc\n");
    }
    AVFrame *frame = av_frame_alloc();
    if (!frame) {
        die("av_frame_alloc\n");
    }
    frame->format = cctx->pix_fmt;    
    frame->width = WIDTH;
    frame->height = HEIGHT;
    ret = av_frame_get_buffer(frame, 0);
    if (ret < 0) {
        die("av_frame_get_buffer: %s\n", av_err2str(ret));
    }
    glViewport(0, 0, WIDTH, HEIGHT);
    for (int step = 0, pts = 0; step < N_STEPS; step++) {
        if (step % (SPS / FPS) == 0) {
            draw();
            glReadPixels(0, 0, WIDTH, HEIGHT, GL_RGB, 
                         GL_UNSIGNED_BYTE, pixels); 
            ret = av_frame_make_writable(frame);
            if (ret < 0) {
                die("av_frame_make_writable: %s\n", av_err2str(ret));
            }
            ret = sws_scale(sws, &src, &src_stride, 0, HEIGHT, 
                            frame->data, frame->linesize);
            frame->pts = pts++;
            encode(cctx, frame, pkt, fmtctx, vid);
        }
        step_sim();
    } 
    encode(cctx, NULL, pkt, fmtctx, vid);
    av_frame_free(&frame);
    av_packet_free(&pkt);
    av_write_trailer(fmtctx);
    avio_close(fmtctx->pb);
    avcodec_free_context(&cctx);
    avformat_free_context(fmtctx);
}

int main(int argc, char **argv) {
    const char *path = "video.mp4";
    if (argc == 2) {
        path = argv[1];
    } else if (argc > 2) {
        die("too many arguments");
    }
    width = WIDTH;
    height = HEIGHT;
    init_draw();
    init_sim();
    create_video(path);
    return 0;
}
