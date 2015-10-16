
#include "Log.h"
#include "bx/platform.h"

#include <cstdlib>
#include <cstdio>

static std::string logFile()
{
#if BX_PLATFORM_LINUX || BX_PLATFORM_OSX
  static const std::string LOG_FILE = "/.zumlog";
  const char * home = getenv("HOME");

  if (home)
    return std::string(home) + LOG_FILE;
  else
    return "~" + LOG_FILE;
#else
  return "log.txt";
#endif
}

void clearLog()
{
  FILE * file = fopen(logFile().c_str(), "w");
  fclose(file);
}

void _logValue(FILE * file, char value)
{
  fprintf(file, "%c", value);
}

void _logValue(FILE * file, int value)
{
  fprintf(file, "%d", value);
}

void _logValue(FILE * file, uint32_t value)
{
  fprintf(file, "%d", value);
}

void _logValue(FILE * file, long value)
{
  fprintf(file, "%ld", value);
}

void _logValue(FILE * file, long long int value)
{
  fprintf(file, "%lld", value);
}

void _logValue(FILE * file, float value)
{
  fprintf(file, "%f", value);
}

void _logValue(FILE * file, double value)
{
  fprintf(file, "%f", value);
}

void _logValue(FILE * file, const char * value)
{
  fprintf(file, "%s", value);
}

void _logValue(FILE * file, Str const& value)
{
  fprintf(file, "%s", value.utf8().c_str());
}

void _logValue(FILE * file, std::string const& value)
{
  fprintf(file, "%s", value.c_str());
}

FILE * _logBegin()
{
  return fopen(logFile().c_str(), "a");
}

void _logEnd(FILE * file)
{
  _logValue(file, "\n");
  fclose(file);
}

void logInfo(Str const& message)
{
  FILE * file = fopen(logFile().c_str(), "a");
  fprintf(file, "INFO: %s\n", message.utf8().c_str());
  fclose(file);
}

void logError(Str const& message)
{
  FILE * file = fopen(logFile().c_str(), "a");
  fprintf(file, "ERROR: %s\n", message.utf8().c_str());
  fclose(file);
}
