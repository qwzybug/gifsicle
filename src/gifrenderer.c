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

Gif_Renderer *Gif_RendererCreate(Gif_Stream *gfs) {
  Gif_Renderer *renderer = malloc(sizeof(Gif_Renderer));
  
  renderer->gfs = gfs;
  
  int width = Gif_ScreenWidth(gfs);
  int height = Gif_ScreenHeight(gfs);
  
  renderer->imageData = calloc(width * height * 3, sizeof(uint8_t));
  renderer->scratchData = calloc(width * height * 3, sizeof(uint8_t));
  
  memset(renderer->imageData, GIF_BACKGROUND, width * height * 3);
  memset(renderer->scratchData, GIF_BACKGROUND, width * height * 3);
  
  renderer->frame = 0;
  renderer->nextFrameWait = 0;
  
  return renderer;
}

void Gif_RendererSetPixel(Gif_Renderer *gr, uint8_t *buf, int x, int y, uint8_t r, uint8_t g, uint8_t b) {
  int width  = Gif_ScreenWidth(gr->gfs);
  int height = Gif_ScreenHeight(gr->gfs);
  
  if (x < 0 || x >= width || y < 0 || y >= height)
    return;
  
  buf[y * width * 3 + x * 3]     = r;
  buf[y * width * 3 + x * 3 + 1] = g;
  buf[y * width * 3 + x * 3 + 2] = b;
}

void Gif_RendererGetPixel(Gif_Renderer *gr, uint8_t *buf, int x, int y, uint8_t *r, uint8_t *g, uint8_t *b) {
  int width  = Gif_ScreenWidth(gr->gfs);
  int height = Gif_ScreenHeight(gr->gfs);
  
  if (x < 0 || x >= width || y < 0 || y >= height)
    return;
  
  *r = buf[y * width * 3 + x * 3];
  *g = buf[y * width * 3 + x * 3 + 1];
  *b = buf[y * width * 3 + x * 3 + 2];
}

void Gif_RendererTick(Gif_Renderer *gr, int ms, Gif_Handler handler) {
  gr->nextFrameWait -= ms;
  if (gr->nextFrameWait > 0)
    return;
  
  // render current frame
  Gif_Image *img = Gif_GetImage(gr->gfs, gr->frame);
  
  uint8_t isTransparent = 0;
  uint8_t r, g, b;
  
  int width = Gif_ScreenWidth(gr->gfs);
  int height = Gif_ScreenHeight(gr->gfs);
  
  for (int y = 0; y < img->height; y++) {
    for (int x = 0; x < img->width; x++) {
      int xpx = x + img->left;
      int ypx = y + img->top;
      
      if (xpx >= width || ypx >= height) {
        isTransparent = 1;
      } else {
        Gif_Color col = Gif_GetColor(gr->gfs, img, x, y, &isTransparent);
        r = col.gfc_red;
        g = col.gfc_green;
        b = col.gfc_blue;
      }
      
      if (isTransparent)
      {
        // use last rendered pixel at this location
        Gif_RendererGetPixel(gr, gr->scratchData, xpx, ypx, &r, &g, &b);
      }
      
      Gif_RendererSetPixel(gr, gr->imageData, xpx, ypx, r, g, b);
      
      switch (img->disposal) {
        case GIF_DISPOSAL_ASIS:
          // set scratch to current value
          Gif_RendererSetPixel(gr, gr->scratchData, xpx, ypx, r, g, b);
          break;
        case GIF_DISPOSAL_BACKGROUND:
          // set scratch to background
          Gif_RendererSetPixel(gr, gr->scratchData, xpx, ypx, GIF_BACKGROUND, GIF_BACKGROUND, GIF_BACKGROUND);
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
