#pragma once
#include <Process/Process.hpp>

#include <score/tools/DeleteAll.hpp>
namespace Process
{
struct ScriptChangeResult
{
  bool valid{};
  score::delete_later<Process::Inlets> inlets;
  score::delete_later<Process::Outlets> outlets;
};
}
