
#pragma once

#include "Str.h"

#include <vector>
#include <functional>

enum class Movement
{
  CellLeft,
  CellRight,
  CellUp,
  CellDown
};

struct EditCommand
{
  uint32_t key[2];
  bool manualRepeat;
  const char * description;
  std::function<void (int)> command;
};

void pushEditCommandKey(uint32_t ch);
void clearEditCommandSequence();
void executeEditCommands();
void executeAppCommands(Str const& commandLine);
Str completeCommand(Str const& command);

std::vector<EditCommand> const& getEditCommands();
