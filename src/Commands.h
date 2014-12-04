
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

struct AppCommand
{
  Str id;
  const char * arg;
  const char * description;
  std::function<void (Str const&)> command;
};

void pushEditCommandKey(uint32_t ch);
void clearEditCommandSequence();
void executeEditCommands();
void executeAppCommands(Str commandLine);

std::vector<EditCommand> const& getEditCommands();
std::vector<AppCommand> const& getAppCommands();
