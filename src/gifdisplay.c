#ifndef _BSD_SOURCE
// or -D _BSD_SOURCE or whatever http://stackoverflow.com/questions/10053788/implicit-declaration-of-function-usleep
#define _BSD_SOURCE
#endif

#include <gifrenderer.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <unistd.h>
#include <signal.h>

const char *program_name = "gifdisplay";

/**
 * Just some dumb kludgy bitmap making
 */

typedef struct {
  int width;
  int height;
  uint8_t *fileHeader;
  uint8_t *infoHeader;
  uint8_t *imageData;
} BMP;

BMP BMPCreate(int width, int height)
{
  BMP bmp;
  
  bmp.width = width;
  bmp.height = height;
  
  bmp.fileHeader = calloc(14, sizeof(uint8_t));
  bmp.infoHeader = calloc(40, sizeof(uint8_t));
  bmp.imageData = calloc(3 * width * height, sizeof(uint8_t));
  
  uint8_t bmpfileheader[14] = {'B','M', 0,0,0,0, 0,0, 0,0, 54,0,0,0};
  uint8_t bmpinfoheader[40] = {40,0,0,0, 0,0,0,0, 0,0,0,0, 1,0, 24,0};
  
  int filesize = width * height;
  
  bmpfileheader[ 2] = (uint8_t)(filesize    );
  bmpfileheader[ 3] = (uint8_t)(filesize>> 8);
  bmpfileheader[ 4] = (uint8_t)(filesize>>16);
  bmpfileheader[ 5] = (uint8_t)(filesize>>24);

  bmpinfoheader[ 4] = (uint8_t)(width    );
  bmpinfoheader[ 5] = (uint8_t)(width>> 8);
  bmpinfoheader[ 6] = (uint8_t)(width>>16);
  bmpinfoheader[ 7] = (uint8_t)(width>>24);
  bmpinfoheader[ 8] = (uint8_t)(height    );
  bmpinfoheader[ 9] = (uint8_t)(height>> 8);
  bmpinfoheader[10] = (uint8_t)(height>>16);
  bmpinfoheader[11] = (uint8_t)(height>>24);
  
  memcpy(bmp.fileHeader, bmpfileheader, 14);
  memcpy(bmp.infoHeader, bmpinfoheader, 40);
  
  return bmp;
}

void BMPFill(BMP bmp, uint8_t r, uint8_t g, uint8_t b) {
  uint8_t pixel[3] = {b, g, r};
  for (int i = 0; i < bmp.width * bmp.height; i++)
  {
    bmp.imageData[i * 3] = b;
    bmp.imageData[i * 3 + 1] = g;
    bmp.imageData[i * 3 + 2] = r;
  }
}

void BMPSetPixel(BMP bmp, int x, int y, uint8_t r, uint8_t g, uint8_t b) {
  bmp.imageData[y * bmp.width * 3 + x * 3] = b;
  bmp.imageData[y * bmp.width * 3 + x * 3 + 1] = g;
  bmp.imageData[y * bmp.width * 3 + x * 3 + 2] = r;
}

void BMPGetPixel(BMP bmp, int x, int y, uint8_t *r, uint8_t *g, uint8_t *b) {
  *b = bmp.imageData[y * bmp.width * 3 + x * 3];
  *g = bmp.imageData[y * bmp.width * 3 + x * 3 + 1];
  *r = bmp.imageData[y * bmp.width * 3 + x * 3 + 2];
}

void BMPWrite(BMP bmp, char *path) {
  FILE *file = fopen(path, "wb");
  
  fwrite(bmp.fileHeader, 1, 14, file);
  fwrite(bmp.infoHeader, 1, 40, file);
  
  uint8_t bmppad[3] = {0, 0, 0};
  for (int i = 0; i < bmp.height; i++)
  {
      fwrite(bmp.imageData + (bmp.width * (bmp.height - i - 1) * 3), 3, bmp.width, file);
      fwrite(bmppad, 1, (4 - (bmp.width * 3) % 4) % 4, file);
  }
  fclose(file);
}

void BMPRelease(BMP bmp) {
  free(bmp.fileHeader);
  free(bmp.infoHeader);
  free(bmp.imageData);
}

/**
 * Some GIF handlers
 */

// scale and crop the gif to a given size, output bitmaps
void scaleCropHandler(Gif_Renderer *gr) {
  int sz = 64;
  
  int minD = MIN(gr->width, gr->height);
  int dx = (gr->width  - minD) / 2;
  int dy = (gr->height - minD) / 2;
  
  float scale = (float)sz / minD;
  
  BMP bitmap = BMPCreate(sz, sz);
  
  uint8_t r, g, b;
  for (int y = 0; y < sz; y++) {
    for (int x = 0; x < sz; x++) {
      int ypx = dy + y / scale;
      int xpx = dx + x / scale;
      
      Gif_RendererGetPixel(gr, gr->imageData, xpx, ypx, &r, &g, &b);
      BMPSetPixel(bitmap, x, y, r, g, b);
    }
  }
  char fname[128];
  sprintf(fname, "out/%d.bmp", gr->frame);
  BMPWrite(bitmap, fname);
  
  BMPRelease(bitmap);
}

// render the whole gif to bitmaps
void simpleHandler(Gif_Renderer *gr) {
  BMP bitmap = BMPCreate(gr->width, gr->height);
  
  uint8_t r, g, b;
  for (int y = 0; y < gr->height; y++) {
    for (int x = 0; x < gr->width; x++) {
      Gif_RendererGetPixel(gr, gr->imageData, x, y, &r, &g, &b);
      BMPSetPixel(bitmap, x, y, r, g, b);
    }
  }
  char fname[128];
  sprintf(fname, "out/%d.bmp", gr->frame);
  BMPWrite(bitmap, fname);
  BMPRelease(bitmap);
}


static int stop = 0;

void sig_handler(int signo)
{
  if (signo == SIGINT)
  {
    printf("STOPPING\n");
    stop = 1;
  }
}

int main(int argc, char *argv[])
{
  // handle SIGINT
  if (signal(SIGINT, sig_handler) == SIG_ERR)
    printf("Can't handle SIGINT\n");
  
  if (argc != 2)
  {
    printf("Usage: gifdisplay foo.gif\n");
    exit(-1);
  }
  
  char *name = argv[1];
  
  FILE *fp = fopen(name, "rb");
  if (fp == NULL) {
    printf("Error opening file %s: %s\n", name, strerror(errno));
    exit(-1);
  }
  
  Gif_Stream *gfs = Gif_ReadFile(fp);
  if (gfs == NULL)
  {
    printf("Error reading gif\n");
    exit(-1);
  }
  
  printf("Displaying %s\n(%d x %d, %d frames)\n", name, Gif_ScreenWidth(gfs), Gif_ScreenHeight(gfs), Gif_ImageCount(gfs));
  
  int ms_per_tick = 5;
  Gif_Renderer *gr = Gif_RendererCreate(gfs, 128, 128);
  while (!stop) {
    Gif_RendererTick(gr, ms_per_tick, scaleCropHandler);
    usleep(ms_per_tick * 1000);
  }
  
  exit(0);
}
