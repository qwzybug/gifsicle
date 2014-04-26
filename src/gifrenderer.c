#include <gifrenderer.h>

#include <string.h>

Gif_Color Gif_GetColorFromColorMap(Gif_Colormap *map, uint8_t val) {
  Gif_Color color;
  if (val < map->ncol)
    color = map->col[val];
  return color;
}

Gif_Color Gif_GetColor(Gif_Stream *gfs, Gif_Image *img, int x, int y, uint8_t *isTransparent) {
  Gif_Color color;
  
  uint8_t val = img->img[y][x];
  
  *isTransparent = 0;
  if (img->transparent >= 0 && val == img->transparent) {
    *isTransparent = 1;
  }
  else if (img->local) {
    color = Gif_GetColorFromColorMap(img->local, val);
  }
  else if (gfs->global) {
    color = Gif_GetColorFromColorMap(gfs->global, val);
  }
  
  return color;
}

/**
 * GIF renderer: render a GIF, call a handler each frame
 */

// Netscape gray
#define GIF_BACKGROUND 0xB3

Gif_Renderer *Gif_RendererCreate(Gif_Stream *gfs, int w, int h) {
  Gif_Renderer *renderer = malloc(sizeof(Gif_Renderer));
  
  renderer->gfs = gfs;
  renderer->width = w;
  renderer->height = h;
  
  int width = Gif_ScreenWidth(renderer->gfs);
  int height = Gif_ScreenHeight(renderer->gfs);
  
  int minD = MIN(width, height);
  renderer->dx = (width  - minD) / 2;
  renderer->dy = (height - minD) / 2;
  
  renderer->scale = MAX((float)w / minD, (float)h / minD);
  
  printf("w %d h %d min %d scale %f dx %d dy %d\n", width, height, minD, renderer->scale, renderer->dx, renderer->dy);
  
  renderer->imageData = calloc(renderer->width * renderer->height * 3, sizeof(uint8_t));
  renderer->scratchData = calloc(renderer->width * renderer->height * 3, sizeof(uint8_t));
  
  memset(renderer->imageData, GIF_BACKGROUND, renderer->width * renderer->height * 3);
  memset(renderer->scratchData, GIF_BACKGROUND, renderer->width * renderer->height * 3);
  
  renderer->frame = 0;
  renderer->nextFrameWait = 0;
  
  return renderer;
}

void Gif_RendererSetPixel(Gif_Renderer *gr, uint8_t *buf, int x, int y, uint8_t r, uint8_t g, uint8_t b) {
  if (x < 0 || x >= gr->width || y < 0 || y >= gr->height)
    return;
  
  // int xpx = gr->dx + x / gr->scale;
  // int ypx = gr->dy + y / gr->scale;
  
  buf[y * gr->width * 3 + x * 3]     = r;
  buf[y * gr->width * 3 + x * 3 + 1] = g;
  buf[y * gr->width * 3 + x * 3 + 2] = b;
}

void Gif_RendererGetPixel(Gif_Renderer *gr, uint8_t *buf, int x, int y, uint8_t *r, uint8_t *g, uint8_t *b) {
  if (x < 0 || x >= gr->width || y < 0 || y >= gr->height)
    return;
  
  *r = buf[y * gr->width * 3 + x * 3];
  *g = buf[y * gr->width * 3 + x * 3 + 1];
  *b = buf[y * gr->width * 3 + x * 3 + 2];
}

void Gif_RendererTick(Gif_Renderer *gr, int ms, Gif_Handler handler) {
  gr->nextFrameWait -= ms;
  if (gr->nextFrameWait > 0)
    return;
  
  // render current frame
  Gif_Image *img = Gif_GetImage(gr->gfs, gr->frame);
  
  uint8_t isTransparent = 0;
  uint8_t r, g, b;
  
  for (int y = 0; y < gr->height; y++) {
    for (int x = 0; x < gr->width; x++) {
      int xpx = x / gr->scale - img->left + gr->dx;
      int ypx = y / gr->scale - img->top  + gr->dy;
      
      if (xpx >= img->width || ypx >= img->height || xpx < img->left || ypx < img->top) {
        isTransparent = 1;
      } else {
        Gif_Color col = Gif_GetColor(gr->gfs, img, xpx, ypx, &isTransparent);
        r = col.gfc_red;
        g = col.gfc_green;
        b = col.gfc_blue;
      }
      
      if (isTransparent)
      {
        // use last rendered pixel at this location
        Gif_RendererGetPixel(gr, gr->scratchData, x, y, &r, &g, &b);
      }
      
      Gif_RendererSetPixel(gr, gr->imageData, x, y, r, g, b);
      
      switch (img->disposal) {
        case GIF_DISPOSAL_ASIS:
          // set scratch to current value
          Gif_RendererSetPixel(gr, gr->scratchData, x, y, r, g, b);
          break;
        case GIF_DISPOSAL_BACKGROUND:
          // set scratch to background
          Gif_RendererSetPixel(gr, gr->scratchData, x, y, GIF_BACKGROUND, GIF_BACKGROUND, GIF_BACKGROUND);
          break;
        case GIF_DISPOSAL_NONE:
        case GIF_DISPOSAL_PREVIOUS:
        default:
          // nothing
          break;
      };
    }
  }
  
  if (handler != NULL) handler(gr);
  
  gr->nextFrameWait = img->delay * 10.0;
  gr->frame = (gr->frame + 1) % Gif_ImageCount(gr->gfs);
}
