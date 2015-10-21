#ifndef __FILM_H__
#define __FILM_H__

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <list>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
}

#include "shot.h"

#include <string>
#include <iostream>

#define abs(x) ((x) < 0 ? -(x) : (x))
#define DEFAULT_THRESHOLD 75

using namespace std;

class shot;
class film {
 private:
  /* Various data for ffmpeg */
  AVFormatContext *pFormatCtx;
  AVCodecContext *pCodecCtx;
  AVCodec *pCodec;
  // Image information for the current and previous frame:
  AVFrame *pFrame;
  // - RGB:
  AVFrame *pFrameRGB;
  AVFrame *pFrameRGBprev;

  AVPacket packet;

  /* Mem allocation */
  int videoStream;

  /* Methods */
  void CompareFrame(AVFrame *pFrame, AVFrame *pFramePrev);

  void update_metadata();

 public:

  string video;
  
  /* Path to the film */
  string input_path;
  /* Film height */
  int height;
  /* Film width */
  int width;
  /* Film fps */
  double fps;
  /* Shots */
  list<shot> shots;
  /* Prev Score in compare_frame */
  int prev_score;
  /* Processing threshold */
  int threshold;
  
  /* Methods */
  int process();

  /* Constructor */
  film();

  /* Accessors */
  inline void set_input_file(string input_file) {
    this->input_path = input_file;
  };
  inline void set_threshold(int threshold) { this->threshold = threshold; };
  inline void set_ipath(string path) { this->input_path = path; };

  inline string get_ipath(void) { return this->input_path; };
  inline double get_fps(void) { return this->fps; };
};

#endif /* !__FILM_H__ */
