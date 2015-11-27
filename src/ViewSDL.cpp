
#include "View.h"
#include "Log.h"
#include "Tcl.h"
#include "Index.h"

#include "UbuntuMono.ttf.h"

#include <vector>
#include <SDL.h>
#include <stb_truetype.h>

namespace view {

  static const tcl::Variable FONT_SIZE("view_fontSize", 16);
  static const tcl::Variable BLINK_RATE("view_cursorBlinkRate", 400);

  struct Glyph
  {
    SDL_Surface * surface = nullptr;
    int x = 0;
    int y = 0;
  };

  struct Cell
  {
    int ch = 0;
    uint16_t fg = COLOR_DEFAULT;
    uint16_t bg = COLOR_DEFAULT;
  };

  struct Color
  {
    uint8_t r;
    uint8_t g;
    uint8_t b;
  };

  static SDL_Window * window_ = nullptr;
  static SDL_Renderer * render_ = nullptr;

  static stbtt_fontinfo font_;
  static int fontBaseline_;
  static int fontLineHeight_;
  static int fontAdvance_;
  static float fontScale_;

  static int width_ = 0;
  static int height_ = 0;
  static Index cursor_;
  static int cursorBlinkTimeout_ = 0;
  static int cursorBlinkVisible_ = true;
  static uint16_t clearForeground_ = COLOR_DEFAULT;
  static uint16_t clearBackground_ = COLOR_DEFAULT;

  //static Color BACKGROUND_COLOR = {39, 40, 34};
  //static Color TEXT_COLOR = {248, 248, 242};
  static Color BACKGROUND_COLOR = {255, 255, 255};
  static Color TEXT_COLOR = {10, 10, 10};

  static std::vector<Glyph> glyphCache_;
  static std::vector<Cell> cells_;

  static bool initializeFont();
  static void initGlyph(int ch);

  bool init(int preferredWidth, int preferredHeight, const char * title)
  {
    int error = SDL_InitSubSystem(SDL_INIT_VIDEO);
    if (error == -1)
    {
      logError("Could not initialize SDL");
      return false;
    }

    if (!initializeFont())
      return false;

    window_ = SDL_CreateWindow(title,
                               SDL_WINDOWPOS_CENTERED,
                               SDL_WINDOWPOS_CENTERED,
                               fontAdvance_ * preferredWidth, fontLineHeight_ * preferredHeight,
                               SDL_WINDOW_RESIZABLE);

    if (window_ == NULL)
    {
      logError("Could not create SDL window: ", SDL_GetError());
      SDL_Quit();
      return false;
    }

    render_ = SDL_CreateRenderer(window_, -1, 0);

    width_ = preferredWidth;
    height_ = preferredHeight;

    cells_.resize(width_ * height_);

    return true;
  }

  void shutdown()
  {
    SDL_DestroyRenderer(render_);
    SDL_DestroyWindow(window_);
    SDL_Quit();
  }

  void setCursor(int x, int y)
  {
    cursor_.x = x;
    cursor_.y = y;
  }

  void hideCursor()
  {
    cursor_.x = -1;
    cursor_.y = -1;
  }

  void setClearAttributes(uint16_t fg, uint16_t bg)
  {
    clearForeground_ = fg;
    clearBackground_ = bg;
  }

  void clear()
  {
    for (uint32_t i = 0; i < cells_.size(); ++i)
    {
      Cell & cell = cells_[i];
      cell.ch = ' ';
      cell.fg = clearForeground_;
      cell.bg = clearBackground_;
    }
  }

  void changeCell(int x, int y, uint32_t ch, uint16_t fg, uint16_t bg)
  {
    if (x < 0 || x >= width_ || y < 0 || y >= height_)
      return;

    int idx = (y * width_) + x;
    Cell & cell = cells_[idx];
    cell.ch = ch;
    cell.fg = fg;
    cell.bg = bg;
  }

  int width()
  {
    return width_;
  }

  int height()
  {
    return height_;
  }

  void present()
  {
    SDL_Surface * screen = SDL_GetWindowSurface(window_);

    SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, BACKGROUND_COLOR.r, BACKGROUND_COLOR.g, BACKGROUND_COLOR.b));

    int xPos = 0;
    int yPos = 0;

    for (int y = 0; y < height_; ++y)
    {
      for (int x = 0; x < width_; ++x)
      {
        Cell & cell = cells_[y * width_ + x];

        if (cell.ch >= glyphCache_.size() || glyphCache_[cell.ch].surface == nullptr)
          initGlyph(cell.ch);

        SDL_Rect rect;
        Glyph & glyph = glyphCache_[cell.ch];

        if (cell.bg & COLOR_REVERSE)
        {
          rect.x = xPos;
          rect.y = yPos;
          rect.w = fontAdvance_;
          rect.h = fontLineHeight_;
          SDL_FillRect(screen, &rect, SDL_MapRGB(screen->format, TEXT_COLOR.r, TEXT_COLOR.g, TEXT_COLOR.b));
        }

        rect.x = xPos + glyph.x;
        rect.y = yPos + fontBaseline_ + glyph.y;

        if (cell.fg & COLOR_REVERSE)
          SDL_SetSurfaceColorMod(glyph.surface, BACKGROUND_COLOR.r, BACKGROUND_COLOR.g, BACKGROUND_COLOR.b);
        else
          SDL_SetSurfaceColorMod(glyph.surface, TEXT_COLOR.r, TEXT_COLOR.g, TEXT_COLOR.b);

        SDL_BlitSurface(glyph.surface, nullptr, screen, &rect);

        xPos += fontAdvance_;
      }

      xPos = 0;
      yPos += fontLineHeight_;
    }

    if (cursor_.x >= 0 && cursor_.y >= 0 && cursorBlinkVisible_)
    {
      SDL_Rect rect;
      rect.x = fontAdvance_ * cursor_.x;
      rect.y = fontLineHeight_ * cursor_.y;
      rect.w = 1;
      rect.h = fontLineHeight_;

      SDL_FillRect(screen, &rect, SDL_MapRGB(screen->format, 255, 255, 255));
    }

    SDL_UpdateWindowSurface(window_);
  }

  inline Keys toKeys(SDL_Keysym symb)
  {
    if (symb.mod & KMOD_CTRL)
    {
      switch (symb.sym)
      {
        case SDLK_2: return KEY_CTRL_2;
        case SDLK_a: return KEY_CTRL_A;
        case SDLK_b: return KEY_CTRL_B;
        case SDLK_c: return KEY_CTRL_C;
        case SDLK_d: return KEY_CTRL_D;
        case SDLK_e: return KEY_CTRL_E;
        case SDLK_f: return KEY_CTRL_F;
        case SDLK_g: return KEY_CTRL_G;
        case SDLK_h: return KEY_CTRL_H;
        case SDLK_i: return KEY_CTRL_I;
        case SDLK_j: return KEY_CTRL_J;
        case SDLK_k: return KEY_CTRL_K;
        case SDLK_l: return KEY_CTRL_L;
        case SDLK_m: return KEY_CTRL_M;
        case SDLK_n: return KEY_CTRL_N;
        case SDLK_o: return KEY_CTRL_O;
        case SDLK_p: return KEY_CTRL_P;
        case SDLK_q: return KEY_CTRL_Q;
        case SDLK_r: return KEY_CTRL_R;
        case SDLK_s: return KEY_CTRL_S;
        case SDLK_t: return KEY_CTRL_T;
        case SDLK_u: return KEY_CTRL_U;
        case SDLK_v: return KEY_CTRL_V;
        case SDLK_w: return KEY_CTRL_W;
        case SDLK_x: return KEY_CTRL_X;
        case SDLK_y: return KEY_CTRL_Y;
        case SDLK_z: return KEY_CTRL_Z;
        case SDLK_3: return KEY_CTRL_3;
        case SDLK_4: return KEY_CTRL_4;
        case SDLK_5: return KEY_CTRL_5;
        case SDLK_6: return KEY_CTRL_6;
        case SDLK_7: return KEY_CTRL_7;
        case SDLK_8: return KEY_CTRL_8;
      }
    }
    else
    {
      switch (symb.sym)
      {
        case SDLK_F1: return KEY_F1;
        case SDLK_F2: return KEY_F2;
        case SDLK_F3: return KEY_F3;
        case SDLK_F4: return KEY_F4;
        case SDLK_F5: return KEY_F5;
        case SDLK_F6: return KEY_F6;
        case SDLK_F7: return KEY_F7;
        case SDLK_F8: return KEY_F8;
        case SDLK_F9: return KEY_F9;
        case SDLK_F10: return KEY_F10;
        case SDLK_F11: return KEY_F11;
        case SDLK_F12: return KEY_F12;
        case SDLK_INSERT: return KEY_INSERT;
        case SDLK_DELETE: return KEY_DELETE;
        case SDLK_HOME: return KEY_HOME;
        case SDLK_END: return KEY_END;
        case SDLK_PAGEUP: return KEY_PGUP;
        case SDLK_PAGEDOWN: return KEY_PGDN;
        case SDLK_UP: return KEY_ARROW_UP;
        case SDLK_DOWN: return KEY_ARROW_DOWN;
        case SDLK_LEFT: return KEY_ARROW_LEFT;
        case SDLK_RIGHT: return KEY_ARROW_RIGHT;
        case SDLK_BACKSPACE: return KEY_BACKSPACE;
        case SDLK_TAB: return KEY_TAB;
        case SDLK_RETURN: return KEY_ENTER;
        case SDLK_ESCAPE: return KEY_ESC;
      }
    }

    return KEY_NONE;
  }

  bool peekEvent(Event * event, int timeout)
  {
    SDL_Event sdlEvent;

    event->type = EVENT_NONE;
    event->key = KEY_NONE;
    event->ch = 0;

    int timeLeft = timeout;
    while (timeLeft > 0)
    {
      if (SDL_PollEvent(&sdlEvent))
      {
        event->type = EVENT_NONE;
        event->key = KEY_NONE;
        event->ch = 0;

        switch (sdlEvent.type)
        {
          case SDL_QUIT:
            event->type = EVENT_QUIT;
            break;

          case SDL_WINDOWEVENT:
            {
              if (sdlEvent.window.event == SDL_WINDOWEVENT_RESIZED)
              {
                width_ = sdlEvent.window.data1 / fontAdvance_;
                height_ = sdlEvent.window.data2 / fontLineHeight_;

                if (width_ == 0) width_ = 1;
                if (height_ == 0) height_ = 1;

                cells_.resize(width_ * height_);

                event->type = EVENT_RESIZE;
              }
            }
            break;

          case SDL_KEYDOWN:
            {
              event->type = EVENT_KEY;
              event->key = toKeys(sdlEvent.key.keysym);

              if (event->key == 0)
              {
                event->type = EVENT_NONE;
                event->key = KEY_NONE;
              }
            }
            break;

          case SDL_TEXTINPUT:
            {
              uint32_t text[4];

              if (str::toUTF32(std::string(sdlEvent.text.text), text, 4) > 0)
              {
                event->type = EVENT_KEY;
                event->ch = text[0];
              }
            }
            break;
        }

        if (event->type != EVENT_NONE)
        {
          // Always reset the cursor blink rate if we press a key
          if (event->type == EVENT_KEY)
          {
            cursorBlinkTimeout_ = 0;
            cursorBlinkVisible_ = true;
          }

          return true;
        }
      }
      else
      {
        SDL_Delay(10);
        timeLeft -= 10;

        cursorBlinkTimeout_ += 10;
        if (cursorBlinkTimeout_ > BLINK_RATE.toInt())
        {
          cursorBlinkVisible_ = !cursorBlinkVisible_;
          cursorBlinkTimeout_ = 0;
        }

        present();
      }
    }

    return false;
  }

  static void initGlyph(int ch)
  {
    Glyph glyph;
    int width, height;

    unsigned char * pixels = stbtt_GetCodepointBitmap(&font_, fontScale_, fontScale_, ch, &width, &height, &glyph.x, &glyph.y);

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    uint32_t rmask = 0xff000000;
    uint32_t gmask = 0x00ff0000;
    uint32_t bmask = 0x0000ff00;
    uint32_t amask = 0x000000ff;
#else
    uint32_t rmask = 0x000000ff;
    uint32_t gmask = 0x0000ff00;
    uint32_t bmask = 0x00ff0000;
    uint32_t amask = 0xff000000;
#endif

    glyph.surface = SDL_CreateRGBSurface(0, width, height, 32, rmask, gmask, bmask, amask);
    SDL_SetSurfaceBlendMode(glyph.surface, SDL_BLENDMODE_BLEND);

    SDL_LockSurface(glyph.surface);

    int * p = (int *)glyph.surface->pixels;

    for (int y = 0; y < height; ++y)
      for (int x = 0; x < width; ++x)
      {
        const int idx = y * width + x;
        p[idx] = SDL_MapRGBA(glyph.surface->format, 255, 255, 255, pixels[idx]);
      }

    SDL_UnlockSurface(glyph.surface);
    stbtt_FreeBitmap(pixels, nullptr);

    if (ch >= glyphCache_.size())
      glyphCache_.resize(ch + 1);

    glyphCache_[ch] = glyph;
  }

  static bool initializeFont()
  {
    if (stbtt_InitFont(&font_, (unsigned char *)UbuntuMono, 0) == 0)
    {
      logError("Could not initialize the font");
      return false;
    }

    // Font size
    int ascent, decent, lineGap, advance;

    fontScale_ = stbtt_ScaleForPixelHeight(&font_, FONT_SIZE.toInt());

    stbtt_GetFontVMetrics(&font_, &ascent, &decent, &lineGap);
    stbtt_GetCodepointHMetrics(&font_, '0', &advance, nullptr);

    fontBaseline_ = ascent * fontScale_;
    fontLineHeight_ = (ascent - decent + lineGap) * fontScale_;
    fontAdvance_ = advance * fontScale_;

    // Initialize some default glyphs
    const Str DEFAULT_GLYPHS(" 0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ,.-;:()=+-*/!\"'#$%&{[]}<>|~");
    for (auto ch : DEFAULT_GLYPHS)
      initGlyph(ch);

    return true;
  }



}
