
#include "View.h"

#include "termbox.h"

namespace view {

  bool init(int preferredWidth, int preferredHeight, const char * title)
  {
      int result = tb_init();
      return (bool)result;
  }

  void shutdown()
  {
    tb_shutdown();
  }

  void setCursor(int x, int y)
  {
    tb_set_cursor(x, y);
  }

  void hideCursor()
  {
    tb_set_cursor(-1, -1);
  }

  void setClearAttributes(uint16_t fg, uint16_t bg)
  {
    tb_set_clear_attributes(fg, bg);
  }

  void changeCell(int x, int y, uint32_t ch, uint16_t fg, uint16_t bg)
  {
    tb_change_cell(x, y, ch, fg, bg); 
  }

  int width()
  {
    return tb_width();
  }

  int height()
  {
    return tb_height();
  }

  void clear()
  {
    tb_clear();
  }

  void present()
  {
    tb_present();
  }

  bool peekEvent(Event * event, int timeout)
  {
    struct tb_event tbEvent;
    int result = tb_peek_event(&tbEvent, timeout);
    if (result)
    {
      event->type = 
    }

    return (bool)result;
  }

}