/* burnscope3.c
 * (c) 2014 Neels Hofmeyr <neels@hofmeyr.de>
 *
 * This file is part of burnscope, published under the GNU General Public
 * License v3.
 */

#include <math.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <time.h>

typedef union {
  Uint8 rgb[3];
  Uint32 v;
} pixel3_t;

#define min(A,B) ((A) > (B)? (B) : (A))
#define max(A,B) ((A) > (B)? (A) : (B))

void rectangle_sum(int add[], pixel3_t *pixbuf, const int W, const int H,
                   const int x, const int y, const int w, const int h,
                   bool wrap_borders) {
  int x_start, x_end;
  int y_start, y_end;
  x_start = x;
  y_start = y;
  x_end = x + w;
  y_end = y + h;

  if (x < 0) {
    if (wrap_borders)
      rectangle_sum(add, pixbuf, W, H, W - (-x), y, -x, h, wrap_borders);
    x_start = 0;
  }
  if (x_end > W) {
    if (wrap_borders)
      rectangle_sum(add, pixbuf, W, H, 0, y, x_end - W, h, wrap_borders);
    x_end = W;
  }

  if (y < 0) {
    if (wrap_borders)
      rectangle_sum(add, pixbuf, W, H, x_start, H - (-y), x_end-x_start, -y, wrap_borders);
    y_start = 0;
  }
  if (y_end > H) {
    if (wrap_borders)
      rectangle_sum(add, pixbuf, W, H, x_start, 0, x_end-x_start, y_end - H, wrap_borders);
    y_end = H;
  }

  pixel3_t *bufpos = &pixbuf[x_start + W*y_start];
  int pitch = W - (x_end - x_start);
  int xpos, ypos;
  for (ypos = y_start; ypos < y_end; ypos ++) {
    for (xpos = x_start; xpos < x_end; xpos ++) {
      int i;
      for (i = 0; i < 3; i++) {
        add[i] += bufpos->rgb[i];
      }
      bufpos ++;
    }
    bufpos += pitch;
  }
}

void surrounding_sum(int add[], pixel3_t *pixbuf, const int W, const int H,
                    const int x, const int y, const int apex_r,
                    bool wrap_borders) {
  rectangle_sum(add, pixbuf, W, H,
                x - apex_r, y - apex_r,
                2 * apex_r + 1,
                2 * apex_r + 1,
                wrap_borders);
}

void burn(pixel3_t *srcbuf, pixel3_t *destbuf, const int W, const int H,
          const int apex_r, float divider, bool wrap_borders) {
  int x, y;
  pixel3_t *destpos = destbuf;
  for (y = 0; y < H; y++) {
    for (x = 0; x < W; x++) {
      int sum[3] = {0, 0, 0};
      surrounding_sum(sum, srcbuf, W, H, x, y, apex_r, wrap_borders);
      int i;
      for (i = 0; i < 3; i ++) {
        int val = (int)( round((float)(sum[i]) / divider) );
        destpos->rgb[i] = (Uint8)val;
      }
      destpos ++;
    }
  }
}


void render(pixel3_t *winbuf, const int winW, const int winH,
            pixel3_t *pixbuf, const int W, const int H,
            int multiply_pixels, SDL_PixelFormat *format)
{
  assert((W * multiply_pixels) == winW);
  assert((H * multiply_pixels) == winH);

  int x, y;
  int mx, my;

  Uint32 *winpos = (Uint32*)(winbuf);
  pixel3_t *pixbufpos = pixbuf;
  for (y = 0; y < H; y++) {
    pixel3_t *pixbuf_y_pos = pixbufpos;
    for (my = 0; my < multiply_pixels; my ++) {
      pixbufpos = pixbuf_y_pos;
      for (x = 0; x < W; x++) {
        pixel3_t p = *pixbufpos;
        int i;
        for (i = 0; i < 3; i ++) {
          if (p.rgb[i] & 0x80)
            p.rgb[i] = 0x7f - (p.rgb[i] & 0x7f);
          p.rgb[i] <<= 1;
        }
        Uint32 val = SDL_MapRGB(format,
                       p.rgb[0],
                       p.rgb[1],
                       p.rgb[2]);
        for (mx = 0; mx < multiply_pixels; mx++) {
          *winpos = val;
          winpos ++;
        }
        pixbufpos ++;
      }
    }
  }
}

void seed(pixel3_t *pixbuf, const int W, const int H, int x, int y,
          pixel3_t *val, bool xsymmetric, bool ysymmetric) {

  int yy = y * W;
  int yys = (H - y - 1) * W;
  pixbuf[x + yy].v += val->v;
  if (xsymmetric) {
    pixbuf[(W - x - 1) + yy].v += val->v;
    if (ysymmetric)
      pixbuf[(W - x - 1) + yys].v += val->v;
  }
  if (ysymmetric)
    pixbuf[x + yys].v += val->v;
}

int main(int argc, char *argv[])
{
  int W = 200;
  int H = 200;
  int multiply_pixels = 2;
  int apex_r = 2;
  float underdampen = .9845;
  int frame_period = 70;
  bool usage = false;
  bool error = false;
  bool asymmetrical = false;
  bool wrap_borders = true;

  int c;

  while (1) {
    c = getopt(argc, argv, "a:g:m:p:u:Abh");
    if (c == -1)
      break;

    switch (c) {
      case 'g':
        {
          char arg[strlen(optarg) + 1];
          strcpy(arg, optarg);
          char *ch = arg;
          while ((*ch) && ((*ch) != 'x')) ch ++;
          if ((*ch) == 'x') {
            *ch = 0;
            ch ++;
            W = atoi(arg);
            H = atoi(ch);

          }
          else {
            fprintf(stderr, "Invalid -g argument: '%s'\n", optarg);
            exit(-1);
          }
        }
        break;

      case 'm':
        multiply_pixels = atoi(optarg);
        break;

      case 'p':
        frame_period = atoi(optarg);
        break;

      case 'a':
        apex_r = atoi(optarg);
        break;

      case 'u':
        underdampen = atof(optarg);
        break;

      case 'b':
        wrap_borders = false;
        break;

      case 'A':
        asymmetrical = true;
        break;

      case '?':
        error = true;
      case 'h':
        usage = true;
        break;

    }
  }

  if (usage) {
    if (error)
      printf("\n");
    printf(
"burnscope3 v0.1\n"
"(c) 2014 Neels Hofmeyr <neels@hofmeyr.de>\n"
"Published under the GNU General Public License v3.\n\n"
"Burnscope produces a mesmerizing animation that I discovered by accident when I\n"
"was a teenager. I've recreated it in memories of old times. It repeatedly\n"
"applies a simple underdamped blur algorithm to a seed image, allowing the color\n"
"values to wrap when overflowing. If you can explain how this staggering\n"
"everchanging complexity can spring from such a simple algorithm and just one\n"
"pixel as seed, please send me an email ;)\n"
"\n"
"burnscope3, other than does not use an indexed palette like burnscope, but\n"
"produces three independent burn scopes in the color channels r, g and b.\n"
"\n"
"Usage example:\n"
"  burnscope -g 320x200 -m 2 -p 70\n"
"\n"
"Options:\n"
"\n"
"  -g WxH  Set animation width and height in number of pixels.\n"
"  -p ms   Set frame period to <ms> milliseconds (slow things down).\n"
"          If zero, run as fast as possible (default).\n"
"  -m N    Multiply each pixel N times in width and height, to give a larger\n"
"          picture. This will also multiply the window size.\n"
"  -a W    Set apex radius, i.e. the blur distance. Default is %d.\n"
"  -u N.n  Set underdampening factor (decimal). Default is %.3f.\n"
"          Reduces normal blur dampening by this factor.\n"
"  -b      Assume zeros around borders. Default is to wrap around borders.\n"
"  -A      Asymmetrical seeding only.\n"
, apex_r, underdampen
);
    if (error)
      return 1;
    return 0;
  }

  const int maxpixels = 1e4;

  if ((W < 3) || (W > maxpixels) || (H < 3) || (H > maxpixels)) {
    fprintf(stderr, "width and/or height out of bounds: %dx%d\n", W, H);
    exit(-1);
  }

  {
    int was_apex_r = apex_r;
    int max_dim = max(W, H);
    apex_r = min(max_dim, apex_r);
    apex_r = max(1, apex_r);
    if (apex_r != was_apex_r) {
      fprintf(stderr, "Invalid apex radius (-a). Forcing %d.", apex_r);
    }
  }

  float minuscule = 1e-3;
  if ((underdampen > -minuscule) && (underdampen < minuscule)) {
    fprintf(stderr, "Underdampening too close to zero (-u). Limit is %f.\n",
        minuscule);
    exit(-1);
  }

  float divider = 1 + 2 * apex_r;
  divider *= divider;
  divider *= underdampen;

  int winW = W;
  int winH = H;

  if (multiply_pixels > 1) {
    winW *= multiply_pixels;
    winH *= multiply_pixels;
  }
  else
    multiply_pixels = 1;

  if ( (winW > maxpixels) || (winH > maxpixels) ) {
    fprintf(stderr, "pixel multiplication is too large: %dx%d times %d = %dx%d\n",
            W, H, multiply_pixels, winW, winH);
    exit(-1);
  }


  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    fprintf(stderr, "Unable to init SDL: %s\n", SDL_GetError());
    exit(1);
  }

  SDL_Window *window;
  window = SDL_CreateWindow("burnscope3", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                            winW, winH, 0);

  if (!window) {
    fprintf(stderr, "Unable to set %dx%d video: %s\n", winW, winH, SDL_GetError());
    exit(1);
  }

  SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, 0);
  if (!renderer) {
    fprintf(stderr, "Unable to set %dx%d video: %s\n", winW, winH, SDL_GetError());
    exit(1);
  }

  SDL_ShowCursor(SDL_DISABLE);
  SDL_PixelFormat *pixelformat = SDL_AllocFormat(SDL_PIXELFORMAT_RGBA8888);
  SDL_Texture *texture = SDL_CreateTexture(renderer, pixelformat->format,
                                           SDL_TEXTUREACCESS_STREAMING, winW, winH);
  if (!texture) {
    fprintf(stderr, "Cannot create texture\n");
    exit(1);
  }

  pixel3_t *buf1 = malloc(W * H * sizeof(pixel3_t));
  pixel3_t *buf2 = malloc(W * H * sizeof(pixel3_t));
  pixel3_t *winbuf = malloc(winW * winH * sizeof(Uint32));
  bzero(buf1, W*H*sizeof(pixel3_t));
  bzero(buf2, W*H*sizeof(pixel3_t));

  pixel3_t *pixbuf = buf1;
  pixel3_t *swapbuf = buf2;

  int rseed = time(NULL);
  printf("random seed: %d\n", rseed);
  srandom(rseed);
  int sym = random() & 3;

  int rgb;
  for (rgb = 0; rgb < 3; rgb ++) {
    pixel3_t val = { .rgb = {0, 0, 0} };
    val.rgb[rgb] = 128;
    int i, j;
    bool xs, ys;
    if (asymmetrical)
      xs = ys = false;
    else {
      xs = ys = true;
      switch((sym + rgb) & 3) {
        case 1:
          xs = false;
          break;
        case 2:
          ys = false;
        default:
          break;
      }
    }
    printf("%c: %s\n", "rgb"[rgb],
           (xs? (ys? "point-symmetrical about center" : "x-symmetrical (about vertical axis)")
              : (ys? "y-symmetrical (about horizontal axis)" : "asymmetrical"))
              );

    j = W * H / 10;
    if (! xs)
      j *= 2;
    if (! ys)
      j *= 2;
    for (i = 0; i < j; i ++) {
      seed(pixbuf, W, H, random() % W, random() % H, &val, xs, ys);
    }
  }

  int last_ticks = SDL_GetTicks() - frame_period;

  while (1)
  {
    bool do_render = false;

    if (frame_period < 1)
      do_render = true;
    else {
      int elapsed = SDL_GetTicks() - last_ticks;
      if (elapsed > frame_period) {
        last_ticks += frame_period * (elapsed / frame_period);
        do_render = true;
      }
    }

    if (do_render) {
      pixel3_t *tmp = swapbuf;
      swapbuf = pixbuf;
      pixbuf = tmp;

#if 1
      burn(swapbuf, pixbuf, W, H, apex_r, divider, wrap_borders);
#else
      int i, y;
      for (y = 0; y < 20; y++) {
        for (i = 0; i < palette.len; i++)
          pixbuf[i + W * y] = i;
      }
#endif
      render(winbuf, winW, winH, pixbuf, W, H, multiply_pixels, pixelformat);

      SDL_UpdateTexture(texture, NULL, winbuf, winW * sizeof(Uint32));

      SDL_RenderClear(renderer);
      SDL_RenderCopy(renderer, texture, NULL, NULL);
      SDL_RenderPresent(renderer);
    }
    else
      SDL_Delay(5);

    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
      switch (event.type)
      {
      case SDL_KEYDOWN:
        // If escape is pressed, return (and thus, quit)
        if (event.key.keysym.sym == SDLK_ESCAPE)
          return 0;
        break;

      case SDL_QUIT:
        return 0;
      }
    }
  }
  return 0;
}
