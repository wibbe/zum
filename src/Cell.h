
#pragma once

#include "Str.h"
#include "Expression.h"

static const uint32_t ALIGN_MASK      = 0x0000000F;
static const uint32_t ALIGN_LEFT      = 0x00000000;
static const uint32_t ALIGN_RIGHT     = 0x00000001;
static const uint32_t ALIGN_CENTER    = 0x00000002;
static const uint32_t FONT_MASK       = 0x000000F0;
static const uint32_t FONT_BOLD       = 0x00000010;
static const uint32_t FONT_UNDERLINE  = 0x00000020;

std::tuple<uint32_t, std::string> parseFormatAndValue(std::string const& str);
std::string formatToStr(uint32_t format);

struct Cell
{
  std::string text;
  std::string display;
  uint32_t format = 0;

  double value = 0.0;
  bool hasExpression = false;
  std::vector<Expr> expression;
};
