
#include "Tcl.h"
#include "Editor.h"

namespace tcl {

  TCL_PROC(string)
  {
    TCL_ARITY(2);

    const uint32_t command = args[1].hash();
    switch (command)
    {
      case kLength:
        {
          if (args.size() != 3)
            return resultInt(0);

          return resultInt(args[2].size());
        }
        break;

      case kIndex:
        {
          TCL_ARITY(3);
          const int idx = args[3].toInt();
          if (idx < 0 || idx >= args[2].size())
            return resultStr(Str::EMPTY);

          return resultStr(Str(args[2][idx]));
        }
        break;

      case kHash:
        {
          TCL_ARITY(2);
          return resultInt(args[2].hash());
        }
        break;

      case kToLower:
        {
          TCL_ARITY(2);
          return resultStr(args[2].toLower());
        }
        break;

      case kToUpper:
        {
          TCL_ARITY(2);
          return resultStr(args[2].toUpper());
        }
        break;

      case kEqual:
        {
          TCL_ARITY(4);

          int length = -1;
          bool ignoreCase = false;

          int i = 2;
          while (i < (args.size() - 2))
          {
            const uint32_t flag = args[i++].hash();

            if (flag == kLength_)
            {
              if (i < (args.size() - 2))
                length = args[i++].toInt();
              else
                return tcl::_reportError(Str("Missing length parameter"));
            }
            else if (flag == kNoCase_)
            {
              ignoreCase = true;
            }
          }

          Str const& str1 = args[args.size() - 2];
          Str const& str2 = args[args.size() - 1];

          if (str1.equals(str2, ignoreCase, length))
            return resultBool(true);
          else
            return resultBool(false);
        }
        break;

      case kIs:
        {
          TCL_ARITY(3);

          switch (args[2].hash())
          {
            case kTrue:
              return resultBool(isTrue(args[3]));

            case kFalse:
              return resultBool(isFalse(args[3]));

            case kAlnum:
              {
                bool valid = true;
                for (auto ch : args[3])
                  valid &= isAlpha(ch) || isDigit(ch);

                return resultBool(valid);
              }
              break;

            case kAlpha:
              {
                bool valid = true;
                for (auto ch : args[3])
                  valid &= isAlpha(ch);

                return resultBool(valid);
              }
              break;

            case kInteger:
              {
                bool valid = true;
                bool first = true;

                for (auto ch : args[3])
                {
                  if (first)
                  {
                    first = false;
                    valid &= isDigit(ch) || ch == '-';
                  }
                  else
                    valid &= isDigit(ch);
                }

                return resultBool(valid);
              }
              break;

            case kBoolean:
              return resultBool(isBoolean(args[3]));

            default:
              return _reportError(Str("Unknown character class '").append(args[2]).append('\''));
          }
        }
        break;

      case kRepeat:
        {
          TCL_ARITY(3);

          int count = args[3].toInt();
          Str result;
          for (; count > 0; --count)
            result.append(args[2]);

          return resultStr(result);
        }

      default:
        break;
    }

    TCL_OK();
  }
}
