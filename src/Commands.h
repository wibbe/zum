
#pragma once

enum class Movement
{
  CellLeft,
  CellRight,
  CellUp,
  CellDown
};

void pushEditCommandKey(uint32_t ch);
void clearEditCommandSequence();
void executeEditCommands();
void executeAppCommands(Str commandLine);