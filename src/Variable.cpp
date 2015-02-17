
#include "Variable.h"

#include <unordered_map>

using VariableRegistry = std::unordered_map<const char *, Str>;

static VariableRegistry & registry()
{
  static VariableRegistry reg;
  return reg;
}


Variable::Variable(const char * name, const char * defaultValue)
  : name_(name),
    defaultValue_(defaultValue)
{
  registry().emplace(name, Str(defaultValue));
}

Str Variable::get() const
{
  const Str value = registry()[name_];
  if (value.empty())
    return Str(defaultValue_);
  return value;
}

void Variable::set(Str const& value) const
{
  registry()[name_] = value;
}

void Variable::set(Str const& name, Str const& value)
{
  registry()[name.utf8().c_str()] = value;
}

Str Variable::get(Str const& name)
{
  return registry()[name.utf8().c_str()];
}
