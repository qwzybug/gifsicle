#include <config.h>
// #include <lcdfgif/gifx.h>
#include <lcdfgif/gif.h>
#include <lcdf/clp.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <math.h>

const char *program_name = "gifdisplay";

#define MIN(X, Y) ((X) < (Y) ? (X) : (Y))
#define MAX(X, Y) ((X) > (Y) ? (X) : (Y))

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

typedef struct {
  Gif_Stream *gfs;
  uint8_t *imageData;
  uint8_t *scratchData;
  unsigned int frame;
  int nextFrameWait;
  // void (*handler)(struct *);
} Gif_Renderer;

typedef void (*Gif_Handler)(Gif_Renderer *);

Gif_Renderer *Gif_RendererCreate(Gif_Stream *gfs) {
  Gif_Renderer *renderer = malloc(sizeof(Gif_Renderer));
  
  renderer->gfs = gfs;
  
  int width = Gif_ScreenWidth(gfs);
  int height = Gif_ScreenHeight(gfs);
  printf("Creating GIF renderer of size %d x %d", width, height);
  
  renderer->imageData = calloc(width * height, sizeof(uint8_t));
  renderer->scratchData = calloc(width * height, sizeof(uint8_t));
  
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
  
  buf[y * width + x]     = r;
  buf[y * width + x + 1] = g;
  buf[y * width + x + 2] = b;
}

void Gif_RendererGetPixel(Gif_Renderer *gr, uint8_t *buf, int x, int y, uint8_t *r, uint8_t *g, uint8_t *b) {
  int width  = Gif_ScreenWidth(gr->gfs);
  int height = Gif_ScreenHeight(gr->gfs);
  
  if (x < 0 || x >= width || y < 0 || y >= height)
    return;
  
  *r = buf[y * width + x];
  *g = buf[y * width + x + 1];
  *b = buf[y * width + x + 2];
}

void Gif_RendererTick(Gif_Renderer *gr, int ms, Gif_Handler handler) {
  gr->nextFrameWait -= ms;
  if (gr->nextFrameWait > 0)
    return;
  
  // render current frame
  Gif_Image *img = Gif_GetImage(gr->gfs, gr->frame);
  
  uint8_t isTransparent = 0;
  uint8_t r, g, b;
  
  for (int y = 0; y < img->height; y++) {
    for (int x = 0; x < img->width; x++) {
      int xpx = x + img->left;
      int ypx = y + img->top;
      if (xpx >= img->width || ypx >= img->top) {
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
      
      // manage image disposal into scratch buffer
      switch (img->disposal) {
        case GIF_DISPOSAL_NONE:
        case GIF_DISPOSAL_ASIS:
          // set scratch to current value
          Gif_RendererSetPixel(gr, gr->scratchData, x, y, r, g, b);
          break;
        case GIF_DISPOSAL_BACKGROUND:
          Gif_RendererSetPixel(gr, gr->scratchData, x, y, GIF_BACKGROUND, GIF_BACKGROUND, GIF_BACKGROUND);
          break;
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

int main(int argc, char *argv[])
{
  unsigned int sz = 128;
  uint8_t bg = 0xB3; // Netscape gray
  
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
  
  int minD = MIN(Gif_ScreenWidth(gfs), Gif_ScreenHeight(gfs));
  
  float scale = (float)sz / minD;
  
  int dx = (Gif_ScreenWidth(gfs)  - minD) / 2;
  int dy = (Gif_ScreenHeight(gfs) - minD) / 2;
  
  BMP bitmap = BMPCreate(sz, sz);
  BMP scratch = BMPCreate(sz, sz);
  
  BMPFill(bitmap, bg, bg, bg);
  BMPFill(scratch, bg, bg, bg);
  
  for (int i = 0; i < Gif_ImageCount(gfs); i++)
  {
    Gif_Image *img = Gif_GetImage(gfs, i);
    printf("Frame %d: %.02fs %d x %d (%d, %d) (disposal %d)\n", i, img->delay / 100.0, img->width, img->height, img->left, img->top, img->disposal);
    
    uint8_t isTransparent = 0;
    uint8_t r, g, b;
    
    for (int y = 0; y < sz; y++)
    {
      for (int x = 0; x < sz; x++)
      {
        int ypx = dy + y / scale - img->top;
        int xpx = dx + x / scale - img->left;
        
        if (ypx < 0 || ypx >= img->height ||
            xpx < 0 || xpx >= img->width) {
          isTransparent = 1;
        }
        else
        {
          Gif_Color col = Gif_GetColor(gfs, img, xpx, ypx, &isTransparent);
          r = col.gfc_red;
          g = col.gfc_green;
          b = col.gfc_blue;
        }
        
        if (isTransparent)
        {
          // use last rendered pixel at this location
          BMPGetPixel(scratch, x, y, &r, &g, &b);
        }
        
        BMPSetPixel(bitmap, x, y, r, g, b);
        
        // manage image disposal into scratch buffer
        switch (img->disposal) {
          case GIF_DISPOSAL_NONE:
          case GIF_DISPOSAL_ASIS:
            // set scratch to current value
            BMPSetPixel(scratch, x, y, r, g, b);
            break;
          case GIF_DISPOSAL_BACKGROUND:
            BMPSetPixel(scratch, x, y, 0xB3, 0xB3, 0xB3);
            break;
          case GIF_DISPOSAL_PREVIOUS:
          default:
            // nothing
            break;
        };
      }
    }
    
    char fname[128];
    sprintf(fname, "out/%d.bmp", i);
    BMPWrite(bitmap, fname);
  }
  
  BMPRelease(bitmap);
  BMPRelease(scratch);
  
  exit(0);
}