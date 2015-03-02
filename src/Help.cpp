
#include "Document.h"
#include "Commands.h"
#include "Tcl.h"

#include <string.h>

static const char * helpStrHeader_ = R"RAW(80

                               ZUM DOCUMENTATION

Zum is a spreadsheet application that can be used to edit CSV files.

The application navigation is influenced by Vim. But a lot of commands
that you would normally find in Vim is absent in Zum.

Zum has three basic modes of operation:

* Navigation Mode - This is the mode the application starts in. This mode is
                    used to traverse the spreadsheet and make modifications to
                    the document.

* Cell Mode - By pressing 'i' while in navigation mode, the application
              switches to cell edit mode. While in this mode it's possible
              to edit the text in the selected cell.

* Command Mode - The application has a build-in Tcl interpreter, that is activated
                 by pressing ':' while in navigation mode.
                 From the command mode it's possible to create, save and load
                 document, and issue a number of different commands to the
                 application.

While in any other mode than navigation the ESCAPE key will cancel the current
activity and jump back into the navigation mode.

Below is a list of all commands that exists in for the different modes.
)RAW";

static const char * helpStrNavCmd_ = R"RAW(
                                NAVIGATION MODE

The commands listed below can be issued while in the navigation mode.

KEY COMBO     DESCRIPTION
)RAW";

static const char * helpStrAppCmd_ = R"RAW(
                                 COMMAND MODE

The commands listed below can be used in the command mode.

NAME                  DESCRIPTION
)RAW";

TCL_FUNC(help)
{
  std::string help(helpStrHeader_);
  help += helpStrNavCmd_;
  for (EditCommand const& cmd : getEditCommands())
  {
    help += (char)cmd.key[0];

    if (cmd.key[1] != 0)
    {
      help += (char)cmd.key[1];
      help += "            ";
    }
    else
      help += "             ";

    help += cmd.description;
    help += "\n";
  }

  doc::loadRaw(help, "[Help]", ';');
  return JIM_OK;
}
