#include <glad/gl.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
#include "balls.h"
#include "draw.h"
#include "misc.h"

#define WIDTH 640
#define HEIGHT 480 

static uint8_t pixels[WIDTH * HEIGHT * 3];
static const char *path = "vid.mp4";

static int next_frame(void) {
    for (int i = 0; i < N_BALLS; i++) 
        scanf("%*d,%f,%f,%f", &balls_pos[i][0], &balls_pos[i][1], &balls_pos[i][2]);
    return !ferror(stdin) && !feof(stdin);
}

static void encode(AVCodecContext *cctx, AVFrame *frame,
                 AVPacket *pkt, AVFormatContext *fmtctx, AVStream *vid) {
    int ret;

    ret = avcodec_send_frame(cctx, frame);
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

static void render(void) {
    struct SwsContext *sws;
    const uint8_t *src;
    int src_stride;
    const AVOutputFormat *outfmt;
    AVFormatContext *fmtctx;
    int ret;
    const AVCodec *codec;
    AVStream *vid;
    AVCodecContext *cctx;
    AVPacket *pkt;
    AVFrame *frame;
    int pts;

    sws = sws_getContext(WIDTH, HEIGHT, AV_PIX_FMT_RGB24,
        WIDTH, HEIGHT, AV_PIX_FMT_NV21, SWS_FAST_BILINEAR,
        NULL, NULL, NULL);
    if (!sws)
        die("sws_getContext\n");
    src = pixels + WIDTH * 3 * (HEIGHT - 1);
    src_stride = -WIDTH * 3;
    outfmt = av_guess_format(NULL, path, NULL);
    if (!outfmt)
        die("av_guess_format\n");
    ret = avformat_alloc_output_context2(&fmtctx, outfmt, NULL, path);
    if (ret < 0)
        die("avformat_free_context: %s\n", av_err2str(ret));
    codec = avcodec_find_encoder(outfmt->video_codec);
    if (!codec)
        die("avcodec_find_encoder\n");
    vid = avformat_new_stream(fmtctx, codec);
    if (!vid)
        die("avformat_new_stream\n");
    cctx = avcodec_alloc_context3(codec);
    if (!cctx)
        die("avcodec_alloc_context3\n");
    vid->codecpar->codec_id = outfmt->video_codec;
    vid->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    vid->codecpar->width = WIDTH;
    vid->codecpar->height = HEIGHT;
    vid->codecpar->format = AV_PIX_FMT_NV21;
    vid->codecpar->bit_rate = 4000000;
    av_opt_set(cctx, "preset", "veryslow", 0);
    avcodec_parameters_to_context(cctx, vid->codecpar);
    cctx->time_base.num = 1; 
    cctx->time_base.den = FPS; 
    cctx->framerate.num = FPS; 
    cctx->framerate.den = 1; 
    cctx->gop_size = 20 * FPS;
    avcodec_parameters_from_context(vid->codecpar, cctx);
    ret = avcodec_open2(cctx, codec, NULL);
    if (ret < 0)
        die("avcodec_open2: %s\n", av_err2str(ret));
    ret = avio_open(&fmtctx->pb, path, AVIO_FLAG_WRITE);
    if (ret < 0)
        die("avio_open: %s\n", av_err2str(ret));
    ret = avformat_write_header(fmtctx, NULL);
    if (ret < 0)
        die("avformat_write_header: %s\n", av_err2str(ret));
    pkt = av_packet_alloc();
    if (!pkt)
        die("av_packet_alloc\n");
    frame = av_frame_alloc();
    if (!frame)
        die("av_frame_alloc\n");
    frame->format = cctx->pix_fmt;    
    frame->width = WIDTH;
    frame->height = HEIGHT;
    ret = av_frame_get_buffer(frame, 0);
    if (ret < 0)
        die("av_frame_get_buffer: %s\n", av_err2str(ret));
    pts = 0;
    vec3 eye = {0.0f, 0.0f, 32.0f};
    vec3 front = {0.0f, 0.0f, -1.0f};
    while (next_frame()) {
        draw(WIDTH, HEIGHT, eye, front);
        glReadPixels(0, 0, WIDTH, HEIGHT, GL_RGB, 
                     GL_UNSIGNED_BYTE, pixels); 
        ret = av_frame_make_writable(frame);
        if (ret < 0)
            die("av_frame_make_writable: %s\n", av_err2str(ret));
        ret = sws_scale(sws, &src,
                &src_stride, 0, HEIGHT, 
                frame->data, frame->linesize);
        frame->pts = pts++;
        encode(cctx, frame, pkt, fmtctx, vid);
    } 
    encode(cctx, NULL, pkt, fmtctx, vid);
    av_frame_free(&frame);
    av_packet_free(&pkt);
    av_write_trailer(fmtctx);
    avio_close(fmtctx->pb);
    avcodec_free_context(&cctx);
    avformat_free_context(fmtctx);
}

static void discard_line(void) {
    int c;
    do {
        c = getchar();
    } while (c != '\n' && c != EOF);
}

int main(int argc, char **argv) {
    switch (argc) {
    case 0:
    case 1:
        break;
    case 3:
        path = argv[2];
        SDL_FALLTHROUGH;
    case 2:
        FILE *tmp = freopen(argv[1], "rb", stdin);
        if (!tmp) {
            perror("freopen");
            exit(EXIT_FAILURE);
        }
        stdin = tmp;
        break;
    default:
        fputs("too many args\n", stderr);
        exit(1);
    }
    init_draw(WIDTH, HEIGHT);
    glViewport(0, 0, WIDTH, HEIGHT);
    discard_line();
    render();
    return 0;
}
