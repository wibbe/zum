
#include "Tcl.h"
#include "Editor.h"

namespace tcl {

  TCL_PROC(string)
  {
    TCL_CHECK_ARG_OLDS(2, 1000, "string subcommand ?argument ...?");

    const uint32_t command = args[1].hash();
    switch (command)
    {
      case kLength:
        {
          TCL_CHECK_ARG_OLD(3, "string length string");

          return resultInt(args[2].size());
        }
        break;

      case kIndex:
        {
          TCL_CHECK_ARG_OLD(4, "string index string idx");

          const int idx = args[3].toInt();
          if (idx < 0 || idx >= args[2].size())
            return resultStr(Str::EMPTY);

          return resultStr(Str(args[2][idx]));
        }
        break;

      case kHash:
        {
          TCL_CHECK_ARG_OLD(3, "string hash string");
          return resultInt(args[2].hash());
        }
        break;

      case kToLower:
        {
          TCL_CHECK_ARG_OLD(3, "string tolower string");
          return resultStr(args[2].toLower());
        }
        break;

      case kToUpper:
        {
          TCL_CHECK_ARG_OLD(3, "string toupper string");
          return resultStr(args[2].toUpper());
        }
        break;

      case kFirst:
        {
          TCL_CHECK_ARG_OLDS(4, 5, "string first needleString haystackString ?startIndex?");

          int start = 0;
          if (args.size() == 5)
            start = args[4].toInt();

          return resultInt(args[3].findStr(args[2], start));
        }
        break;

      case kEqual:
        {
          TCL_CHECK_ARG_OLDS(4, 7, "string equals ?-length length? ?-nocase? string1 string2");

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
                return tcl::argError("string equals ?-length length? ?-nocase? string1 string2");
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
          TCL_CHECK_ARG_OLD(4, "string is class string");

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
          TCL_CHECK_ARG_OLD(4, "string repeat string count");

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
