
#include "Tcl.h"
#include "Document.h"

namespace tcl {

  TCL_PROC(index)
  {
    TCL_CHECK_ARGS(2, 1000, "index subcommand ?argument ...?");

    const uint32_t command = args[1].hash();
    switch (command)
    {
      case kNew:
        {
          TCL_CHECK_ARG(4, "index new column row");
          return resultStr(doc::toCellRef(Index(args[2].toInt(), args[3].toInt())));
        }
        break;

      case kRow:
        {
          TCL_CHECK_ARG(3, "index row index");
          return resultInt(doc::parseCellRef(args[2]).y);
        }
        break;

      case kColumn:
        {
          TCL_CHECK_ARG(3, "index column index");
          return resultInt(doc::parseCellRef(args[2]).x);
        }
        break;

      default:
        break;
    }

    TCL_OK();
  }
}
