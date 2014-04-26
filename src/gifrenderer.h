#ifndef __GIFRENDERER_H__
#define __GIFRENDERER_H__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <inttypes.h>
#include <lcdfgif/gif.h>

#define MIN(X, Y) ((X) < (Y) ? (X) : (Y))
#define MAX(X, Y) ((X) > (Y) ? (X) : (Y))

typedef struct {
  Gif_Stream *gfs;
  uint8_t *imageData;
  uint8_t *scratchData;
  unsigned int frame;
  int nextFrameWait;
} Gif_Renderer;

typedef void (*Gif_Handler)(Gif_Renderer *);

Gif_Color Gif_GetColorFromColorMap(Gif_Colormap *map, uint8_t val);
Gif_Color Gif_GetColor(Gif_Stream *gfs, Gif_Image *img, int x, int y, uint8_t *isTransparent);

Gif_Renderer *Gif_RendererCreate(Gif_Stream *gfs);

void Gif_RendererGetPixel(Gif_Renderer *gr, uint8_t *buf, int x, int y, uint8_t *r, uint8_t *g, uint8_t *b);
void Gif_RendererTick(Gif_Renderer *gr, int ms, Gif_Handler handler);

#endif
