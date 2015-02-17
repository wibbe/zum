
#include "Help.h"
#include "Commands.h"

#include <string.h>

static const char * helpStrHeader_ = R"RAW(80

                               ZUM DOCUMENTATION

Zum is a spreadsheet application that can be used to edit CSV files.

The navigation in the application is heavily influenced by Vim. But this does
not mean every command in Vim maps to a corresponding command in Zum.

Zum has three editing modes:

* Navigation Mode - This is the mode the application starts in. This mode is
                    used to traverse the spreadsheet and make modifications to
                    the document.

* Cell Edit Mode - By pressing 'i' in navigation mode the application switches
                   to cell edit mode. It is then possible to edit the content of
                   a single cell in the document.

* Command Mode - The command mode is entered by typing ':'. From there it is
                 possible to enter commands that affect the whole application.

By pressing the ESCAPE key in either cell-edit or command mode the application
switches back into the navigation mode.

Below is a list of all commands that exists in for the different modes.
)RAW";

static const char * helpStrNavCmd_ = R"RAW(
                                NAVIGATION MODE

The commands listed below can be used in the navigation mode.

KEY COMBO     DESCRIPTION
)RAW";

static const char * helpStrAppCmd_ = R"RAW(
                                 COMMAND MODE

The commands listed below can be used in the command mode.

NAME                  DESCRIPTION
)RAW";


std::string getHelpDocument()
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

/*
  help += helpStrAppCmd_;
  for (AppCommand const& cmd : getAppCommands())
  {
    help += ":" + cmd.id.utf8();
    help += " " + std::string(cmd.arg);

    int spacing = 20 - cmd.id.size() - strlen(cmd.arg);
    for (; spacing > 0; --spacing)
      help += " ";

    help += cmd.description;
    help += "\n";
  }
*/
  return help;
}