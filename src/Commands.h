
#pragma once

enum class Movement
{
  CellLeft,
  CellRight,
  CellUp,
  CellDown
};

void pushCommandKey(uint32_t ch);
void executeCommandLine();