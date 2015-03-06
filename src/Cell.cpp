
#include "Cell.h"

static const std::string START = "#{";
static const std::string END = "}";

std::tuple<uint32_t, std::string> parseFormatAndValue(std::string const& str)
{
  const std::size_t startPos = str.find(START);
  const std::size_t endPos = str.find(END, startPos);

  if (startPos == std::string::npos || endPos == std::string::npos)
    return std::make_tuple(0, str);

  const uint32_t format = std::stod(str.substr(startPos + START.size(), endPos - startPos - START.size()));
  const std::string value = str.substr(0, startPos);
  return std::make_tuple(format, value);
}

std::string formatToStr(uint32_t format)
{
  if (format == 0)
    return "";

  return START + str::fromInt(format) + END;
}