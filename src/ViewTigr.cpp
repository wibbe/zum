
#include "View.h"
#include "Log.h"
#include "Tcl.h"
#include "Index.h"

#include "UbuntuMono.ttf.h"

#include <vector>
#include <tigr.h>
#include <stb_truetype.h>

namespace view {

  static const tcl::Variable FONT_SIZE("view_fontSize", 16);
  static const tcl::Variable BLINK_RATE("view_cursorBlinkRate", 400);

  struct Glyph
  {
    //SDL_Surface * surface = nullptr;
    Tigr * surface = nullptr;
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

  //static SDL_Window * _window = nullptr;
  static Tigr * _window = nullptr;

  static stbtt_fontinfo _font;
  static int _fontBaseline;
  static int _fontLineHeight;
  static int _fontAdvance;
  static float _fontScale;

  static int _width = 0;
  static int _height = 0;
  static Index _cursor;
  static int _cursorBlinkTimeout = 0;
  static int _cursorBlinkVisible = true;
  static uint16_t _clearForeground = COLOR_TEXT;
  static uint16_t _clearBackground = COLOR_BACKGROUND;

  static Color BACKGROUND_COLOR = {39, 40, 34};
  static Color TEXT_COLOR = {248, 248, 242};

  static std::vector<Glyph> _glyphCache;
  static std::vector<Cell> _cells;

  static std::vector<Event> _eventQueue;

  static bool initializeFont();
  static void initGlyph(int ch);

  bool init(int preferredWidth, int preferredHeight, const char * title)
  {
    if (!initializeFont())
      return false;

    _window = tigrWindow(_fontAdvance * preferredWidth, _fontLineHeight * preferredHeight, title, TIGR_RETINA | TIGR_AUTO);
    if (_window == NULL)
    {
      logError("Could not create window");
      return false;
    }

    tigrUpdate(_window);

    _width = _window->w / _fontAdvance;
    _height = _window->h / _fontLineHeight;

    _cells.resize(_width * _height);
    return true;
  }

  void shutdown()
  {
    tigrFree(_window);
  }

  void setCursor(int x, int y)
  {
    _cursor.x = x;
    _cursor.y = y;
  }

  void hideCursor()
  {
    _cursor.x = -1;
    _cursor.y = -1;
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

  inline TPixel colorFromEnum(uint16_t color)
  {
    switch (color & 0x00FF)
    {
      case COLOR_BACKGROUND:  return tigrRGB(33, 37, 43);
      case COLOR_PANEL:       return tigrRGB(40, 44, 52);
      case COLOR_HIGHLIGHT:   return tigrRGB(82, 139, 255);
      case COLOR_TEXT:        return tigrRGB(174, 182, 195);
      case COLOR_SELECTION:   return tigrRGB(44, 50, 60);
      case COLOR_WHITE:       return tigrRGB(255, 255, 255);
      default:                return tigrRGB(255, 255, 255);
    }
  }

  void present()
  {
    tigrFill(_window, 0, 0, _window->w, _window->h, colorFromEnum(COLOR_BACKGROUND));

    int xPos = 0;
    int yPos = 0;

    for (int y = 0; y < _height; ++y)
    {
      for (int x = 0; x < _width; ++x)
      {
        Cell & cell = _cells[y * _width + x];

        if (cell.ch >= _glyphCache.size() || _glyphCache[cell.ch].surface == nullptr)
          initGlyph(cell.ch);

        Glyph & glyph = _glyphCache[cell.ch];

        if (cell.bg != COLOR_DEFAULT)
        {
          tigrFill(_window, xPos, yPos, _fontAdvance, _fontLineHeight, colorFromEnum(cell.bg));
        }

        //if (cell.fg & COLOR_REVERSE)
        //  textColor = tigrRGB(BACKGROUND_COLOR.r, BACKGROUND_COLOR.g, BACKGROUND_COLOR.b);

        tigrBlitTint(_window, glyph.surface, xPos + glyph.x, yPos + _fontBaseline + glyph.y, 0, 0, _fontAdvance, _fontLineHeight, colorFromEnum(cell.fg));

        xPos += _fontAdvance;
      }

      xPos = 0;
      yPos += _fontLineHeight;
    }

    if (_cursor.x >= 0 && _cursor.y >= 0 && _cursorBlinkVisible)
    {
      tigrLine(_window, _fontAdvance * _cursor.x, _fontLineHeight * _cursor.y,
                        _fontAdvance * _cursor.x, _fontLineHeight * (_cursor.y + 1),
                        colorFromEnum(COLOR_HIGHLIGHT));
    }
  }

  inline Keys handleCtrl(uint32_t symb)
  {
    switch (symb)
    {
      case 'a': return KEY_CTRL_A;
      case 'b': return KEY_CTRL_B;
      case 'c': return KEY_CTRL_C;
      case 'd': return KEY_CTRL_D;
      case 'e': return KEY_CTRL_E;
      case 'f': return KEY_CTRL_F;
      case 'g': return KEY_CTRL_G;
      case 'h': return KEY_CTRL_H;
      case 'i': return KEY_CTRL_I;
      case 'j': return KEY_CTRL_J;
      case 'k': return KEY_CTRL_K;
      case 'l': return KEY_CTRL_L;
      case 'm': return KEY_CTRL_M;
      case 'n': return KEY_CTRL_N;
      case 'o': return KEY_CTRL_O;
      case 'p': return KEY_CTRL_P;
      case 'q': return KEY_CTRL_Q;
      case 'r': return KEY_CTRL_R;
      case 's': return KEY_CTRL_S;
      case 't': return KEY_CTRL_T;
      case 'u': return KEY_CTRL_U;
      case 'v': return KEY_CTRL_V;
      case 'w': return KEY_CTRL_W;
      case 'x': return KEY_CTRL_X;
      case 'y': return KEY_CTRL_Y;
      case 'z': return KEY_CTRL_Z;
      case '2': return KEY_CTRL_2;
      case '3': return KEY_CTRL_3;
      case '4': return KEY_CTRL_4;
      case '5': return KEY_CTRL_5;
      case '6': return KEY_CTRL_6;
      case '7': return KEY_CTRL_7;
      case '8': return KEY_CTRL_8;
    }
    return KEY_NONE;
  }

  struct TigrToKey
  {
    int tigr;
    Keys key;
  };

  static const TigrToKey KEYS[] = {
    { TK_F1, KEY_F1 },
    { TK_F2, KEY_F2 },
    { TK_F3, KEY_F3 },
    { TK_F4, KEY_F4 },
    { TK_F5, KEY_F5 },
    { TK_F6, KEY_F6 },
    { TK_F7, KEY_F7 },
    { TK_F8, KEY_F8 },
    { TK_F9, KEY_F9 },
    { TK_F10, KEY_F10 },
    { TK_F11, KEY_F11 },
    { TK_F12, KEY_F12 },
    { TK_INSERT, KEY_INSERT },
    { TK_DELETE, KEY_DELETE },
    { TK_HOME, KEY_HOME },
    { TK_END, KEY_END },
    { TK_PAGEUP, KEY_PGUP },
    { TK_PAGEDN, KEY_PGDN },
    { TK_UP, KEY_ARROW_UP },
    { TK_DOWN, KEY_ARROW_DOWN },
    { TK_LEFT, KEY_ARROW_LEFT },
    { TK_RIGHT, KEY_ARROW_RIGHT },
    { TK_BACKSPACE, KEY_BACKSPACE },
    { TK_TAB, KEY_TAB },
    { TK_RETURN, KEY_ENTER },
    { TK_ESCAPE, KEY_ESC },
    { 0, KEY_NONE },
  };

  bool peekEvent(Event * event, int timeout)
  {
    //SDL_Event sdlEvent;


    if (_eventQueue.size() > 0)
    {
      *event = _eventQueue.front();
      _eventQueue.erase(_eventQueue.begin());
      return true;
    }

    int timeLeft = timeout;
    while (timeLeft > 0)
    {
      float startTime = tigrTime();

      int lastW = _window->w;
      int lastH = _window->h;
      tigrUpdate(_window);

      // Did the window size change?
      if (lastW != _window->w || lastH != _window->h)
      {
        _width = _window->w / _fontAdvance;
        _height = _window->h / _fontLineHeight;

        if (_width == 0) _width = 1;
        if (_height == 0) _height = 1;

        _cells.resize(_width * _height);

        Event e = {EVENT_RESIZE, KEY_NONE, 0};
        _eventQueue.push_back(e);
      }

      // Did we close the window?
      if (tigrClosed(_window) > 0)
      {
        Event e = {EVENT_QUIT, KEY_NONE, 0};
        _eventQueue.push_back(e);
      }

      // Keyboard input?
      if (tigrKeyHeld(_window, TK_RCONTROL) || tigrKeyHeld(_window, TK_LCONTROL))
      {
        Keys key = handleCtrl(tigrReadChar(_window));
        if (key != KEY_NONE)
          _eventQueue.push_back(Event {EVENT_KEY, key, 0});
      }
      else
      {
        uint32_t ch = tigrReadChar(_window);
        if (ch >  0 && ch != 10 && ch != 27 && ch != 8 && ch != 9)
        {
          Event e = {EVENT_KEY, KEY_NONE, ch};
          logInfo("Ch: ", (int)ch);
          _eventQueue.push_back(e);
        }

        for (int i = 0; KEYS[i].key != KEY_NONE; ++i)
        {
          if (tigrKeyDown(_window, KEYS[i].tigr))
            _eventQueue.push_back(Event {EVENT_KEY, KEYS[i].key, 0});
        }
      }

      float endTime = tigrTime();
      int dt = (endTime - startTime) * 1000;
      timeLeft -= dt;
    }


    /*
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
                _width = sdlEvent.window.data1 / _fontAdvance;
                _height = sdlEvent.window.data2 / _fontLineHeight;

                if (_width == 0) _width = 1;
                if (_height == 0) _height = 1;

                _cells.resize(_width * _height);

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
      }
      else
      {
        SDL_Delay(10);
        timeLeft -= 10;

        _cursorBlinkTimeout += 10;
        if (_cursorBlinkTimeout > BLINK_RATE.toInt())
        {
          _cursorBlinkVisible = !_cursorBlinkVisible;
          _cursorBlinkTimeout = 0;
        }

        present();
      }
    }

    if (event->type != EVENT_NONE)
    {
    // Always reset the cursor blink rate if we press a key
    if (event->type == EVENT_KEY)
    {
    _cursorBlinkTimeout = 0;
    _cursorBlinkVisible = true;
  }

  return true;
}
*/

    return false;
  }

  static void initGlyph(int ch)
  {
    Glyph glyph;
    int width, height;

    unsigned char * pixels = stbtt_GetCodepointBitmap(&_font, _fontScale, _fontScale, ch, &width, &height, &glyph.x, &glyph.y);

    glyph.surface = tigrBitmap(width, height);

    TPixel * p = glyph.surface->pix;

    for (int y = 0; y < height; ++y)
      for (int x = 0; x < width; ++x)
      {
        const int idx = y * width + x;
        p[idx] = tigrRGBA(255, 255, 255, pixels[idx]);
      }

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

    _fontScale = stbtt_ScaleForPixelHeight(&_font, FONT_SIZE.toInt());

    stbtt_GetFontVMetrics(&_font, &ascent, &decent, &lineGap);
    stbtt_GetCodepointHMetrics(&_font, '0', &advance, nullptr);

    _fontBaseline = ascent * _fontScale;
    _fontLineHeight = (ascent - decent + lineGap) * _fontScale;
    _fontAdvance = advance * _fontScale;

    // Initialize some default glyphs
    const Str DEFAULT_GLYPHS(" 0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ,.-;:()=+-*/!\"'#$%&{[]}<>|~");
    for (auto ch : DEFAULT_GLYPHS)
      initGlyph(ch);

    return true;
  }



}
