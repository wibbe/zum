
#pragma once

#include "Str.h"
#include <string>

void clearLog();

void _logValue(FILE * file, char value);
void _logValue(FILE * file, int value);
void _logValue(FILE * file, long value);
void _logValue(FILE * file, long long int value);
void _logValue(FILE * file, float value);
void _logValue(FILE * file, double value);
void _logValue(FILE * file, const char * value);
void _logValue(FILE * file, Str const& value);
void _logValue(FILE * file, std::string const& value);
FILE * _logBegin();
void _logEnd(FILE * file);

template <typename T, typename ...U>
void _logValue(FILE * file, T const& t, U ...u)
{
  _logValue(file, t);
  _logValue(file, u...);
}

template <typename ...T>
void logInfo(T const& ...t)
{
  FILE * file = _logBegin();
  _logValue(file, t...);
  _logEnd(file);
}

template <typename ...T>
void logError(T const& ...t)
{
  FILE * file = _logBegin();
  _logValue(file, "Error: ");
  _logValue(file, t...);
  _logEnd(file);
}
