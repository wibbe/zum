
#include "View.h"
#include "Log.h"
#include "Tcl.h"
#include "Index.h"

#include "UbuntuMono.ttf.h"

#include <vector>
#include <stb_truetype.h>
#include <GLFW/glfw3.h>
#include <sera.h>

#define GL_BGRA 0x80E1

namespace view {

  static const tcl::Variable FONT_SIZE("view_fontSize", 16);
  static const tcl::Variable BLINK_RATE("view_cursorBlinkRate", 400);

  struct Glyph
  {
    sr_Buffer * surface = nullptr;
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

  static GLFWwindow * _window = nullptr;
  static sr_Buffer * _canvas = nullptr;
  static uint32_t _canvasTexture = 0;

  static stbtt_fontinfo _font;
  static int _fontBaseline;
  static int _fontLineHeight;
  static int _fontLinePadding = 2;
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
  static void keyboardEvent(int event, int key);

  static void errorCallback(int error, const char * description);
  static void windowSizeCallback(GLFWwindow * window, int width, int height);
  static void keyCallback(GLFWwindow * window, int key, int scancode, int action, int mods);
  static void inputCallback(GLFWwindow * window, unsigned int codePoint);


  bool init(int preferredWidth, int preferredHeight, const char * title)
  {
    if (!initializeFont())
      return false;

    if (!glfwInit())
      return false;

    glfwSetErrorCallback(errorCallback);

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 1);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    _window = glfwCreateWindow(_fontAdvance * preferredWidth, _fontLineHeight * preferredHeight, title, nullptr, nullptr);
    if (_window == NULL)
    {
      logError("Could not create window");
      glfwTerminate();
      return false;
    }

    glfwSetWindowSizeCallback(_window, windowSizeCallback);
    glfwSetKeyCallback(_window, keyCallback);
    glfwSetCharCallback(_window, inputCallback);

    glfwMakeContextCurrent(_window);

    {
      int w, h;
      glfwGetWindowSize(_window, &w, &h);
      _width = w / _fontAdvance;
      _height = h / _fontLineHeight;

      _canvas = sr_newBuffer(w, h);
    }

    glEnable(GL_TEXTURE_2D);
    glGenTextures(1, &_canvasTexture);
    glBindTexture(GL_TEXTURE_2D, _canvasTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    _cells.resize(_width * _height);

    return true;
  }

  void shutdown()
  {
    if (_window)
    {
      sr_destroyBuffer(_canvas);
      glfwDestroyWindow(_window);

      _canvas = nullptr;
      _window = nullptr;
    }

    glfwTerminate();
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

  inline sr_Pixel colorFromEnum(uint16_t color)
  {
    switch (color & 0x00FF)
    {
      case COLOR_BACKGROUND:  return sr_color(33, 37, 43);
      case COLOR_PANEL:       return sr_color(40, 44, 52);
      case COLOR_HIGHLIGHT:   return sr_color(82, 139, 255);
      case COLOR_TEXT:        return sr_color(178, 186, 199);
      case COLOR_SELECTION:   return sr_color(44, 50, 60);
      case COLOR_WHITE:       return sr_color(255, 255, 255);
      default:                return sr_color(255, 255, 255);
    }
  }

  void present()
  {
    int xPos = 0;
    int yPos = 0;
    sr_Pixel whiteColor = sr_color(255, 255, 255);

    sr_reset(_canvas);
    sr_clear(_canvas, colorFromEnum(COLOR_BACKGROUND));


    for (int y = 0; y < _height; ++y)
    {
      for (int x = 0; x < _width; ++x)
      {
        Cell & cell = _cells[y * _width + x];

        if (cell.ch >= _glyphCache.size() || (cell.ch != 32 && _glyphCache[cell.ch].surface == nullptr))
          initGlyph(cell.ch);

        Glyph & glyph = _glyphCache[cell.ch];

        if (cell.bg != COLOR_DEFAULT)
        {
          sr_setColor(_canvas, whiteColor);
          sr_drawRect(_canvas, colorFromEnum(cell.bg), xPos, yPos, _fontAdvance, _fontLineHeight);
        }

        if (cell.ch != 32)
        {
          //if (cell.fg & COLOR_REVERSE)
          //  textColor = tigrRGB(BACKGROUND_COLOR.r, BACKGROUND_COLOR.g, BACKGROUND_COLOR.b);

          sr_setColor(_canvas, colorFromEnum(cell.fg));
          sr_drawBuffer(_canvas, glyph.surface, xPos + glyph.x, yPos + _fontBaseline + glyph.y + (_fontLinePadding / 2), nullptr, nullptr); //colorFromEnum(cell.fg));
        }

        xPos += _fontAdvance;
      }

      xPos = 0;
      yPos += _fontLineHeight;
    }

    if (_cursor.x >= 0 && _cursor.y >= 0 && _cursorBlinkVisible)
    {
      sr_setColor(_canvas, whiteColor);
      sr_drawLine(_canvas, whiteColor,
                  _fontAdvance * _cursor.x, _fontLineHeight * _cursor.y,
                  _fontAdvance * _cursor.x, _fontLineHeight * (_cursor.y + 1));
    }
    
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    int windowWidth, windowHeight;
    glfwGetWindowSize(_window, &windowWidth, &windowHeight);  

    glViewport(0, 0, windowWidth, windowHeight);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, windowWidth, windowHeight, 0.0, 1.0, -1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glBindTexture(GL_TEXTURE_2D, _canvasTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, _canvas->w, _canvas->h, 0, GL_BGRA, GL_UNSIGNED_BYTE, _canvas->pixels);

    glBegin(GL_QUADS);
      glTexCoord2f(0.0f, 0.0f); glVertex2f(0.0f, 0.0f);
      glTexCoord2f(1.0f, 0.0f); glVertex2f(windowWidth, 0.0f);
      glTexCoord2f(1.0f, 1.0f); glVertex2f(windowWidth, windowHeight);
      glTexCoord2f(0.0f, 1.0f); glVertex2f(0.0f, windowHeight);
    glEnd();

    glfwSwapBuffers(_window);
  }

  inline Keys glfwToCtrl(int symb)
  {
    switch (symb)
    {
      case GLFW_KEY_A: return KEY_CTRL_A;
      case GLFW_KEY_B: return KEY_CTRL_B;
      case GLFW_KEY_C: return KEY_CTRL_C;
      case GLFW_KEY_D: return KEY_CTRL_D;
      case GLFW_KEY_E: return KEY_CTRL_E;
      case GLFW_KEY_F: return KEY_CTRL_F;
      case GLFW_KEY_G: return KEY_CTRL_G;
      case GLFW_KEY_H: return KEY_CTRL_H;
      case GLFW_KEY_I: return KEY_CTRL_I;
      case GLFW_KEY_J: return KEY_CTRL_J;
      case GLFW_KEY_K: return KEY_CTRL_K;
      case GLFW_KEY_L: return KEY_CTRL_L;
      case GLFW_KEY_M: return KEY_CTRL_M;
      case GLFW_KEY_N: return KEY_CTRL_N;
      case GLFW_KEY_O: return KEY_CTRL_O;
      case GLFW_KEY_P: return KEY_CTRL_P;
      case GLFW_KEY_Q: return KEY_CTRL_Q;
      case GLFW_KEY_R: return KEY_CTRL_R;
      case GLFW_KEY_S: return KEY_CTRL_S;
      case GLFW_KEY_T: return KEY_CTRL_T;
      case GLFW_KEY_U: return KEY_CTRL_U;
      case GLFW_KEY_V: return KEY_CTRL_V;
      case GLFW_KEY_W: return KEY_CTRL_W;
      case GLFW_KEY_X: return KEY_CTRL_X;
      case GLFW_KEY_Y: return KEY_CTRL_Y;
      case GLFW_KEY_Z: return KEY_CTRL_Z;
      case GLFW_KEY_2: return KEY_CTRL_2;
      case GLFW_KEY_3: return KEY_CTRL_3;
      case GLFW_KEY_4: return KEY_CTRL_4;
      case GLFW_KEY_5: return KEY_CTRL_5;
      case GLFW_KEY_6: return KEY_CTRL_6;
      case GLFW_KEY_7: return KEY_CTRL_7;
      case GLFW_KEY_8: return KEY_CTRL_8;
    }
    return KEY_NONE;
  }

  inline Keys glfwToKey(int key)
  {
    switch (key)
    {
      case GLFW_KEY_F1: return KEY_F1;
      case GLFW_KEY_F2: return KEY_F2;
      case GLFW_KEY_F3: return KEY_F3;
      case GLFW_KEY_F4: return KEY_F4;
      case GLFW_KEY_F5: return KEY_F5;
      case GLFW_KEY_F6: return KEY_F6;
      case GLFW_KEY_F7: return KEY_F7;
      case GLFW_KEY_F8: return KEY_F8;
      case GLFW_KEY_F9: return KEY_F9;
      case GLFW_KEY_F10: return KEY_F10;
      case GLFW_KEY_F11: return KEY_F11;
      case GLFW_KEY_F12: return KEY_F12;
      case GLFW_KEY_INSERT: return KEY_INSERT;
      case GLFW_KEY_DELETE: return KEY_DELETE;
      case GLFW_KEY_HOME: return KEY_HOME;
      case GLFW_KEY_END: return KEY_END;
      case GLFW_KEY_PAGE_UP: return KEY_PGUP;
      case GLFW_KEY_PAGE_DOWN: return KEY_PGDN;
      case GLFW_KEY_UP: return KEY_ARROW_UP;
      case GLFW_KEY_DOWN: return KEY_ARROW_DOWN;
      case GLFW_KEY_LEFT: return KEY_ARROW_LEFT;
      case GLFW_KEY_RIGHT: return KEY_ARROW_RIGHT;
      case GLFW_KEY_BACKSPACE: return KEY_BACKSPACE;
      case GLFW_KEY_TAB: return KEY_TAB;
      case GLFW_KEY_ENTER: return KEY_ENTER;
      case GLFW_KEY_ESCAPE: return KEY_ESC;
    }

    return KEY_NONE;
  }

  static void keyCallback(GLFWwindow * window, int key, int scancode, int action, int mods)
  {
    if (action == GLFW_PRESS || action == GLFW_REPEAT)
    {
      Keys k = KEY_NONE;
      if (mods & GLFW_MOD_CONTROL)
        k = glfwToCtrl(key);
      else
        k = glfwToKey(key);

      if (k != KEY_NONE)
        _eventQueue.push_back(Event {EVENT_KEY, k, 0});

      _cursorBlinkVisible = true;
      _cursorBlinkTimeout = 0;
    }
  }

  static void inputCallback(GLFWwindow * window, unsigned int codePoint)
  {
    _cursorBlinkVisible = true;
    _cursorBlinkTimeout = 0;
    _eventQueue.push_back(Event {EVENT_KEY, KEY_NONE, (uint32_t)codePoint});
  }

  static void errorCallback(int error, const char * description)
  {
    logError("Error ", error, " ", description);
  }

  static void windowSizeCallback(GLFWwindow * window, int width, int height)
  {
    // Make sure we only add once resize event
    if (_eventQueue.size() == 0 || _eventQueue.back().type != EVENT_RESIZE)
    {
      Event e = {EVENT_RESIZE, KEY_NONE, 0};
      _eventQueue.push_back(e);
    }

    _width = width / _fontAdvance;
    _height = height / _fontLineHeight;

    if (_width == 0) _width = 1;
    if (_height == 0) _height = 1;

    _cells.resize(_width * _height);

    sr_destroyBuffer(_canvas);
    _canvas = sr_newBuffer(width, height);
  }

  void waitEvent(Event * event)
  {
    // If we have pending events to process, we poll for new events and then return the oldest one
    if (_eventQueue.size() > 0)
    {
      glfwPollEvents();
      *event = _eventQueue.front();
      _eventQueue.erase(_eventQueue.begin());
      return;
    }

    while (_eventQueue.size() == 0)
    {
      glfwWaitEventsTimeout(0.01);

      if (glfwWindowShouldClose(_window))
      {
        Event e = {EVENT_QUIT, KEY_NONE, 0};
        _eventQueue.push_back(e);
      }

      _cursorBlinkTimeout += 10;
      if (_cursorBlinkTimeout > BLINK_RATE.toInt())
      {
        _cursorBlinkVisible = !_cursorBlinkVisible;
        _cursorBlinkTimeout = 0;
        present();
      }
    }

    *event = _eventQueue.front();
    _eventQueue.erase(_eventQueue.begin());
  }

  static void initGlyph(int ch)
  {
    Glyph glyph;
    int width, height;

    unsigned char * pixels = stbtt_GetCodepointBitmap(&_font, _fontScale, _fontScale, ch, &width, &height, &glyph.x, &glyph.y);

    // Ignore the whitespace character
    if (ch != 32)
    {
      glyph.surface = sr_newBuffer(width, height);

      for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
        {
          const int idx = y * width + x;
          glyph.surface->pixels[idx] = sr_pixel(255, 255, 255, pixels[idx]);
        }
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
    _fontLineHeight = (ascent - decent + lineGap) * _fontScale + _fontLinePadding;
    _fontAdvance = advance * _fontScale;

    // Initialize some default glyphs
    const Str DEFAULT_GLYPHS(" 0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ,.-;:()=+-*/!\"'#$%&{[]}<>|~");
    for (auto ch : DEFAULT_GLYPHS)
      initGlyph(ch);

    return true;
  }
}
