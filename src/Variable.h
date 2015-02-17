
#pragma once

#include "Str.h"
#include <functional>

class Variable
{
  public:
    Variable(const char * name, const char * defaultValue);

    const char * name() const { return name_; }

    Str get() const;
    void set(Str const& value) const;

    static Str get(Str const& name);
    static void set(Str const& name, Str const& value);

  private:
    const char * name_;
    const char * defaultValue_;
};

namespace std
{
  template<>
  struct hash<Variable>
  {
    typedef Variable argument_type;
    typedef std::size_t result_type;

    result_type operator()(argument_type const& s) const
    {
      return std::hash<const char *>()(s.name());
    }
  };
}