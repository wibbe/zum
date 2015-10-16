
#include "View.h"
#include "Log.h"
#include "Tcl.h"

#include "UbuntuMono.ttf.h"

#include <vector>
#include <SDL.h>
#include <stb_truetype.h>

namespace view {

  static const tcl::Variable FONT_SIZE("view_fontSize", 15);

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

  static SDL_Window * _window = nullptr;
  
  static stbtt_fontinfo _font;
  static int _fontBaseline;
  static int _fontLineHeight;
  static int _fontAdvance;

  static int _width = 0;
  static int _height = 0;
  static uint16_t _clearForeground = COLOR_DEFAULT;
  static uint16_t _clearBackground = COLOR_DEFAULT;

  static std::vector<Glyph> _glyphCache;
  static std::vector<Cell> _cells;

  static bool initializeFont();

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

    _window = SDL_CreateWindow(title,
                               SDL_WINDOWPOS_CENTERED,
                               SDL_WINDOWPOS_CENTERED,
                               _fontAdvance * preferredWidth, _fontLineHeight * preferredHeight,
                               SDL_WINDOW_RESIZABLE);

    if (_window == NULL)
    {
      logError("Could not create SDL window: ", SDL_GetError());
      SDL_Quit();
      return false;
    }

    _width = preferredWidth;
    _height = preferredHeight;

    _cells.resize(_width * _height);

    return true;
  }

  void shutdown()
  {
    SDL_DestroyWindow(_window);
    SDL_Quit();
  }

  void setCursor(int x, int y)
  {

  }

  void hideCursor()
  {

  }

  void setClearAttributes(uint16_t fg, uint16_t bg)
  {
    _clearForeground = fg;
    _clearBackground = bg;
  }

  void clear()
  {
    for (uint32_t i = 0; i < _cells.size(); ++i)
    {
      Cell & cell = _cells[i];
      cell.ch = ' ';
      cell.fg = _clearForeground;
      cell.bg = _clearBackground;
    }
  }

  void changeCell(int x, int y, uint32_t ch, uint16_t fg, uint16_t bg)
  {
    if (x < 0 || x >= _width || y < 0 || y >= _height)
      return;

    int idx = (y * _width) + x;
    Cell & cell = _cells[idx];
    cell.ch = ch;
    cell.fg = fg;
    cell.bg = bg;
  }

  int width()
  {
    return _width;
  }

  int height()
  {
    return _height;
  }

  void present()
  {
    SDL_Surface * screen = SDL_GetWindowSurface(_window);

    SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, 0, 0, 0));

    const char * msg = "Hello World 123%4/{}+,l.";

    int xPos = 0;
    int yPos = 0;

    for (int y = 0; y < _height; ++y)
    {
      for (int x = 0; x < _width; ++x)
      {
        Cell & cell = _cells[y * _width + x];
        if (cell.ch < _glyphCache.size())
        {
          SDL_Rect rect;
          Glyph & glyph = _glyphCache[cell.ch];

          if (cell.bg & COLOR_REVERSE)
          {
            rect.x = xPos;
            rect.y = yPos;
            rect.w = _fontAdvance;
            rect.h = _fontLineHeight;
            SDL_FillRect(screen, &rect, SDL_MapRGB(screen->format, 255, 255, 255));
          }

          rect.x = xPos + glyph.x;
          rect.y = yPos + _fontBaseline + glyph.y;

          if (cell.fg & COLOR_REVERSE)
            SDL_SetSurfaceColorMod(glyph.surface, 0, 0, 0);

          SDL_BlitSurface(glyph.surface, nullptr, screen, &rect);
        }


        xPos += _fontAdvance;
      }

      xPos = 0;
      yPos += _fontLineHeight;
    }
/*
    while (*msg)
    {
      Glyph & g = _glyphCache[*msg];

      SDL_Rect rect;
      rect.x = x;
      rect.y = y;
      rect.w = _fontAdvance;
      rect.h = _fontLineHeight;
      SDL_FillRect(screen, &rect, SDL_MapRGB(screen->format, 255, 255, 255));

      rect.x = x + g.x;
      rect.y = y + _fontBaseline + g.y;
      SDL_SetSurfaceColorMod(g.surface, 0, 0, 0);
      SDL_BlitSurface(g.surface, nullptr, screen, &rect);

      x += _fontAdvance;


      ++msg;
    }
    */

    SDL_UpdateWindowSurface(_window);
  }

  bool peekEvent(Event * event, int timeout)
  {
    SDL_Event sdlEvent;

    event->type = EVENT_NONE;
    event->key = KEY_NONE;
    event->ch = 0;

    if (SDL_WaitEventTimeout(&sdlEvent, timeout))
    {
      switch (sdlEvent.type)
      {
        case SDL_QUIT:
          event->type = EVENT_QUIT;
          break;

        case SDL_WINDOWEVENT:
          {
            if (sdlEvent.window.event == SDL_WINDOWEVENT_RESIZED)
            {
              _width = sdlEvent.window.data1 / _fontAdvance;
              _height = sdlEvent.window.data2 / _fontLineHeight;

              if (_width == 0) _width = 1;
              if (_height == 0) _height = 1;

              _cells.resize(_width * _height);

              event->type = EVENT_RESIZE;
            }
          }
          break;
      }     
    }

    return event->type != EVENT_NONE;
  }

  static void initGlyph(int ch, float scale)
  {
    Glyph glyph;
    int width, height;

    unsigned char * pixels = stbtt_GetCodepointBitmap(&_font, scale, scale, ch, &width, &height, &glyph.x, &glyph.y);

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

    for (int y = 0; y < height; ++y)
      for (int x = 0; x < width; ++x)
      {
        const int idx = y * width + x;

        int * p = (int *)glyph.surface->pixels;
        p[idx] = SDL_MapRGBA(glyph.surface->format, 255, 255, 255, pixels[idx]);
      }

    SDL_UnlockSurface(glyph.surface);
    stbtt_FreeBitmap(pixels, nullptr);

    if (ch >= _glyphCache.size())
      _glyphCache.resize(ch + 1);

    _glyphCache[ch] = glyph;
  }

  static bool initializeFont()
  {
    if (stbtt_InitFont(&_font, (unsigned char *)UbuntuMono, 0) == 0)
    {
      logError("Could not initialize the font");
      return false;
    }

    // Font size
    int ascent, decent, lineGap, advance;

    float scale = stbtt_ScaleForPixelHeight(&_font, FONT_SIZE.toInt());

    stbtt_GetFontVMetrics(&_font, &ascent, &decent, &lineGap);
    stbtt_GetCodepointHMetrics(&_font, '0', &advance, nullptr);

    _fontBaseline = ascent * scale;
    _fontLineHeight = (ascent - decent + lineGap) * scale;
    _fontAdvance = advance * scale;

    // Initialize some default glyphs
    const char * defaultGlyphs = " 0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ,.-;:()=+-*/!\"'#$%&{[]}<>|";
    while (*defaultGlyphs)
    {
      initGlyph(*defaultGlyphs, scale);
      ++defaultGlyphs;
    }

    return true;
  }



}
