#include <sys/time.h>
#include <time.h>
#include "film.h"

extern "C" {
#include <libswscale/swscale.h>
}

/*
 * This function gathers the RGB values per frame and evaluates the
 * possibility if this frame is a detected shot.
 */
void film::CompareFrame(AVFrame *pFrame, AVFrame *pFramePrev) {
  int x;
  int y;
  int diff;
  int frame_number = pCodecCtx->frame_number;
  int c1, c2, c3;
  int c1tot, c2tot, c3tot;
  c1tot = 0;
  c2tot = 0;
  c3tot = 0;
  int c1prev, c2prev, c3prev;
  int score;
  score = 0;

  // IDEA! Split image in slices and calculate score per-slice.
  // This would allow to detect areas on the image which have stayed
  // the same, and (a) increase score if all areas have changed
  // and (b) decrease score if some areas have changed less (ot not at all).
  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++) {
      c1 = *(pFrame->data[0] + y * pFrame->linesize[0] + x * 3);
      c2 = *(pFrame->data[0] + y * pFrame->linesize[0] + x * 3 + 1);
      c3 = *(pFrame->data[0] + y * pFrame->linesize[0] + x * 3 + 2);

      c1prev = *(pFramePrev->data[0] + y * pFramePrev->linesize[0] + x * 3);
      c2prev = *(pFramePrev->data[0] + y * pFramePrev->linesize[0] + x * 3 + 1);
      c3prev = *(pFramePrev->data[0] + y * pFramePrev->linesize[0] + x * 3 + 2);

      c1tot += int((char)c1 + 127);
      c2tot += int((char)c2 + 127);
      c3tot += int((char)c3 + 127);

      score += abs(c1 - c1prev);
      score += abs(c2 - c2prev);
      score += abs(c3 - c3prev);
    }
  }
  int nbpx = (height * width);

  /*
   * On se ramene à la moyenne
   */
  score /= nbpx;
  c1tot /= nbpx;
  c2tot /= nbpx;
  c3tot /= nbpx;

  /*
   * Calculate numerical difference between this and the previous frame
   */
  diff = abs(score - prev_score);
  prev_score = score;

  /*
   * Take care of storing frame position and images of detecte scene cut
   */
  if ((diff > this->threshold) && (score > this->threshold)) {
    shot s;
    s.msbegin = int((frame_number * 1000) / fps);
    s.myid = shots.back().myid + 1;
    s.score = score;
    shots.push_back(s);
  }
}

void film::update_metadata() {
  char buf[256];
  /* Video metadata */
  if (videoStream != -1) {
    this->height = int(pFormatCtx->streams[videoStream]->codec->height);
    this->width = int(pFormatCtx->streams[videoStream]->codec->width);
    this->fps = av_q2d(pFormatCtx->streams[videoStream]->r_frame_rate);
    avcodec_string(buf, sizeof(buf), pFormatCtx->streams[videoStream]->codec,
                   0);
    this->video = buf;

  } else {
    this->video = "null";
    this->height = 0;
    this->width = 0;
    this->fps = 0;
  }
}

int film::process() {
  int frameFinished;
  shot s;
  static struct SwsContext *img_convert_ctx = NULL;
  static struct SwsContext *img_ctx = NULL;
  int frame_number;

  /*
   * Register all formats and codecs
   */
  av_register_all();
  pFormatCtx = avformat_alloc_context();
  if (avformat_open_input(&pFormatCtx, input_path.c_str(), NULL, NULL) != 0) {
    printf("Could not open file %s\n", input_path.c_str());
    return -1;  // Couldn't open file
  }

  /*
   * Retrieve stream information
   */
  if (avformat_find_stream_info(pFormatCtx, NULL) < 0)
    return -1;  // Couldn't find stream information

  av_dump_format(pFormatCtx, 0, input_path.c_str(), false);
  videoStream = -1;

  /*
   * Detect streams types
   */
  for (int j = 0; j < pFormatCtx->nb_streams; j++) {
    switch (pFormatCtx->streams[j]->codec->codec_type) {
      case AVMEDIA_TYPE_VIDEO:
        videoStream = j;
        break;

      default:
        break;
    }
  }

  update_metadata();

  /*
   * Find the decoder for the video stream
   */
  if (videoStream != -1) {
    pCodecCtx = pFormatCtx->streams[videoStream]->codec;
    pCodec = avcodec_find_decoder(pCodecCtx->codec_id);

    if (pCodec == NULL) return -1;  // Codec not found
    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0)
      return -1;  // Could not open codec

    /*
     * Allocate current and previous video frames
     */
    pFrame = avcodec_alloc_frame();
    // RGB:
    pFrameRGB = avcodec_alloc_frame();      // current frame
    pFrameRGBprev = avcodec_alloc_frame();  // previous frame

    /*
     * Allocate memory for the pixels of a picture and setup the AVPicture
     * fields for it
     */
    // RGB:
    avpicture_alloc((AVPicture *)pFrameRGB, PIX_FMT_RGB24, width, height);
    avpicture_alloc((AVPicture *)pFrameRGBprev, PIX_FMT_RGB24, width, height);

    /*
     * Mise en place du premier plan
     */
    s.msbegin = 0;
    s.myid = 0;
    s.score = 100;
    shots.push_back(s);
  }

  /*
   * Main loop to control the movie processing flow
   */
  while (av_read_frame(pFormatCtx, &packet) >= 0) {
    if (packet.stream_index == videoStream) {
      avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);

      if (frameFinished) {
        frame_number = pCodecCtx->frame_number;  // Current frame number

        // Convert the image into YUV444
        if (!img_ctx) {
          img_ctx =
              sws_getContext(width, height, pCodecCtx->pix_fmt, width, height,
                             PIX_FMT_YUV444P, SWS_BICUBIC, NULL, NULL, NULL);
          if (!img_ctx) {
            fprintf(stderr,
                    "Cannot initialize the converted YUV image context!\n");
            exit(1);
          }
        }

        // Convert the image into RGB24
        if (!img_convert_ctx) {
          img_convert_ctx =
              sws_getContext(width, height, pCodecCtx->pix_fmt, width, height,
                             PIX_FMT_RGB24, SWS_BICUBIC, NULL, NULL, NULL);
          if (!img_convert_ctx) {
            fprintf(stderr,
                    "Cannot initialize the converted RGB image context!\n");
            exit(1);
          }
        }

        /*
         * Calling "sws_scale" is used to copy the data from "pFrame->data" to
         *other
         * frame buffers for later processing. It is also used to convert
         *between
         * different pix_fmts.
         *
         * API: int sws_scale(SwsContext *c, uint8_t *src, int srcStride[], int
         *srcSliceY, int srcSliceH, uint8_t dst[], int dstStride[] )
        */
        sws_scale(img_convert_ctx, pFrame->data, pFrame->linesize, 0,
                  pCodecCtx->height, pFrameRGB->data, pFrameRGB->linesize);

        /* If it's not the first image */
        if (frame_number != 1) {
          CompareFrame(pFrameRGB, pFrameRGBprev);
        }
        
        /* Copy current frame as "previous" for next round */
        av_picture_copy((AVPicture *)pFrameRGBprev, (AVPicture *)pFrameRGB,
                        PIX_FMT_RGB24, width, height);
      }
    }
    /*
     * Free the packet that was allocated by av_read_frame
     */
    if (packet.data != NULL) av_free_packet(&packet);
  }

  if (videoStream != -1) {
    /*
     * Free the RGB images
     */
    av_free(pFrame);
    av_free(pFrameRGB);
    av_free(pFrameRGBprev);
    avcodec_close(pCodecCtx);
  }

  // Close the video file
  avformat_close_input(&pFormatCtx);
  return 0;
}

film::film() {
  // Initialization of default values (non GUI)
  threshold = DEFAULT_THRESHOLD;
}
