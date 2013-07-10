#ifndef _POSIXSHM_COMMON_H_
#define _POSIXSHM_COMMON_H_

#include <framework/mlt.h>

struct posix_shm_header {
  uint32_t frame;
  uint32_t frame_rate_num;
  uint32_t frame_rate_den;
  uint32_t image_size;
  mlt_image_format image_format;
  uint32_t width;
  uint32_t height;
  uint32_t audio_size;
  mlt_audio_format audio_format;
  uint32_t frequency;
  uint32_t channels;
  uint32_t samples;
};

#endif
