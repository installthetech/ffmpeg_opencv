// Include Libraries
#include <iostream>
#include <vector>
#include <math.h>

// FFmpeg libraries
extern "C" {
  #include <libavformat/avformat.h>
  #include <libavcodec/avcodec.h>
  #include <libavutil/avutil.h>
  #include <libavutil/pixdesc.h>
  #include <libswscale/swscale.h>

  #include <libavutil/opt.h>
  #include <libavcodec/avcodec.h>
  #include <libavutil/channel_layout.h>
  #include <libavutil/common.h>
  #include <libavutil/imgutils.h>
  #include <libavutil/mathematics.h>
  #include <libavutil/samplefmt.h>
}

// OpenCV libraries
#include <opencv2/opencv.hpp>

using namespace cv;
using namespace std;


#define CAM_INDEX 0 // for opening the default web camera
#define NUM_FRAMES 250 // number of frames to be encoded
// Functions protyping
static void init(const char *filename, int codec_id);
static void process(const char *filename, int codec_id);
static void finish(const char *filename, int codec_id);


// Global Variables
AVCodec *codec;
AVCodecContext *c= NULL;
int i, ret, x, y, got_output;
FILE *f;
AVFrame *frame;
AVPacket pkt;
uint8_t endcode[] = { 0, 0, 1, 0xb7 };
SwsContext* swsctx;


// init() - runs at the start
static void init(const char *filename, int codec_id)
{
  // search for the codec, if available
  codec = avcodec_find_encoder(static_cast<AVCodecID>(codec_id));
  if (!codec) {
    fprintf(stderr, "Codec unavailable\n");
    exit(1);
  }
  c = avcodec_alloc_context3(codec);
  if (!c) {
    fprintf(stderr, "Could not allocate context\n");
    exit(1);
  }

  // Setting sample parameters
  c->bit_rate = 6400000;

  // Video frame size
  c->width = 640;
  c->height = 480;

  // setting fps
  c->time_base= (AVRational){1,25};
  c->gop_size = 10; /* emit one intra frame every ten frames */
  c->max_b_frames=1;
  c->pix_fmt = AV_PIX_FMT_YUV420P;
  if(codec_id == AV_CODEC_ID_H264)
    av_opt_set(c->priv_data, "preset", "slow", 0);

  // Open the codec
  if (avcodec_open2(c, codec, NULL) < 0) {
    fprintf(stderr, "Could not open codec\n");
    exit(1);
  }
  f = fopen(filename, "wb");
  if (!f) {
    fprintf(stderr, "Could not open %s\n", filename);
    exit(1);
  }

  //TODO Uncomment for FFmpeg version < 3
  frame = avcodec_alloc_frame();

  //TODO Uncomment for FFmpeg version >= 3
  //frame = av_frame_alloc();

  if (!frame) {
    fprintf(stderr, "Could not allocate frame for video\n");
    exit(1);
  }

  frame->format = c->pix_fmt;
  frame->width  = c->width;
  frame->height = c->height;

  ret = av_image_alloc(frame->data, frame->linesize, c->width, c->height,
  c->pix_fmt, 32);
  if (ret < 0) {
    fprintf(stderr, "Could not allocate raw picture buffer\n");
    exit(1);
  }

}

// process() - runs continuously during program
static void process(int i, Mat image)
{
  // Filling frame data
  resize(image, image, Size(frame->width, frame->height));
  swsctx = sws_getCachedContext(nullptr, frame->width, frame->height, AV_PIX_FMT_BGR24,frame->width, frame->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, nullptr, nullptr, nullptr);


  av_init_packet(&pkt);
  pkt.data = NULL;
  pkt.size = 0;
  fflush(stdout);


  frame->pts = i;
  const int stride[] = { static_cast<int>(image.step[0]) };
  sws_scale(swsctx, &image.data, stride, 0, image.rows, frame->data, frame->linesize);

  // encoding image
  ret = avcodec_encode_video2(c, &pkt, frame, &got_output);
  if (ret < 0) {
    fprintf(stderr, "Error encoding frame\n");
    exit(1);
  }
  if (got_output) {
    fwrite(pkt.data, 1, pkt.size, f);
    av_free_packet(&pkt);
  }


}

static void finish()
{
  // Last frame
  fflush(stdout);
  ret = avcodec_encode_video2(c, &pkt, NULL, &got_output);
  if (ret < 0) {
    fprintf(stderr, "Error encoding frame\n");
    exit(1);
  }
  if (got_output) {
    printf("Write frame %3d (size=%5d)\n", i, pkt.size);
    fwrite(pkt.data, 1, pkt.size, f);
    av_free_packet(&pkt);
  }

  // Closing parameters
  fwrite(endcode, 1, sizeof(endcode), f);
  fclose(f);
  avcodec_close(c);
  av_free(c);
  av_freep(&frame->data[0]);

  //TODO Uncomment for FFmpeg version < 3
  avcodec_free_frame(&frame);
  //TODO Uncomment for FFmpeg version >= 3
  //av_frame_free(&frame);

}

int main(int argc, char** argv)
{
  const char *output_type;

  VideoCapture cap(CAM_INDEX);
  Mat frame;
  /* register all the codecs */
  avcodec_register_all();
  init("test.mpg", AV_CODEC_ID_MPEG1VIDEO);
  for(int i = 0; i<NUM_FRAMES; i++)
  {
    cap>>frame;
    process(i, frame);
  }
  finish();
  return 0;
}
