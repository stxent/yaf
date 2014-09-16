/*
 * timestamps.hpp
 * Copyright (C) 2014 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef TIMESTAMPS_HPP_
#define TIMESTAMPS_HPP_
//------------------------------------------------------------------------------
#include "shell.hpp"
//------------------------------------------------------------------------------
class CurrentDate : public Shell::ShellCommand
{
public:
  CurrentDate(Shell &parent) :
      ShellCommand(parent)
  {
  }

  virtual const char *name() const
  {
    return "date";
  }

  virtual result run(unsigned int, const char * const *);
};
//------------------------------------------------------------------------------
class MeasureTime : public Shell::ShellCommand
{
public:
  MeasureTime(Shell &parent) :
      ShellCommand(parent)
  {
  }

  virtual const char *name() const
  {
    return "time";
  }

  virtual result run(unsigned int, const char * const *);
};
//------------------------------------------------------------------------------
#endif //TIMESTAMPS_HPP_
