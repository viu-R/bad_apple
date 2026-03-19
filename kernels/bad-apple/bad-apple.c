#include <am.h>
#include <stdio.h>
#include <klib-macros.h>

#define FPS 30
#define CHAR_LEVELS " .:-=+*#%@"

typedef struct {
  uint8_t pixel[VIDEO_ROW * VIDEO_COL / 8];
} frame_t;

static void sleep_until(uint64_t next) {
  while (io_read(AM_TIMER_UPTIME).us < next) ;
}

static uint8_t getbit(uint8_t *p, int idx) {
  if (idx < 0 || idx >= VIDEO_ROW * VIDEO_COL) return 0;
  int byte_idx = idx / 8;
  int bit_idx = idx % 8;
  bit_idx = 7 - bit_idx;
  uint8_t byte = p[byte_idx];
  uint8_t bit = (byte >> bit_idx) & 1;
  return bit;
}

int main() {
  extern uint8_t video_payload, video_payload_end;
  extern uint8_t audio_payload, audio_payload_end;
  int audio_len = 0, audio_left = 0;
  Area sbuf;
  ioe_init();
  frame_t *f = (void *)&video_payload;
  frame_t *fend = (void *)&video_payload_end;
  printf("\033[H\033[J");
  bool has_audio = io_read(AM_AUDIO_CONFIG).present;
  if (has_audio) {
    io_write(AM_AUDIO_CTRL, AUDIO_FREQ, AUDIO_CHANNEL, 1024);
    audio_left = audio_len = &audio_payload_end - &audio_payload;
    sbuf.start = &audio_payload;
  }
  
  uint64_t now = io_read(AM_TIMER_UPTIME).us;
  for (; f < fend; f ++) {
     printf("\033[0;0H");
     for (int y = 0; y < VIDEO_ROW - 1; y += 2) {
       for (int x = 0; x < VIDEO_COL - 1; x += 2) {
         int count = 0;
         count += getbit(f->pixel, y * VIDEO_COL + x);
         count += getbit(f->pixel, y * VIDEO_COL + x + 1);
         count += getbit(f->pixel, (y + 1) * VIDEO_COL + x);
         count += getbit(f->pixel, (y + 1) * VIDEO_COL + x + 1);
         
         int level = count * 9 / 5; 
         if (level > 9) level = 9;
         putch(CHAR_LEVELS[level]);
       }
       putch('\n');
     }
     if (has_audio) {
       int should_play = (AUDIO_FREQ / FPS) * sizeof(int16_t) * AUDIO_CHANNEL;
       if (should_play > audio_left) should_play = audio_left;
       audio_left -= should_play;
       while (should_play > 0) {
         int len = (should_play > 4096 ? 4096 : should_play);
         sbuf.end = sbuf.start + len;
         io_write(AM_AUDIO_PLAY, sbuf);
         sbuf.start += len;
         should_play -= len;
       }
     }
     uint64_t next = now + (1000 * 1000 / FPS);
     sleep_until(next);
     now = next;
  }
  return 0;
}
