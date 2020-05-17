#pragma once
#include <score_lib_base_export.h>
namespace score
{
class Command;
struct SCORE_LIB_BASE_EXPORT Dispatcher
{
  virtual ~Dispatcher();
  virtual void submit(score::Command*) = 0;
};

template <typename T>
struct Dispatcher_T final : Dispatcher
{
  explicit Dispatcher_T(T& t) : impl{t} { }
  T& impl;
  void submit(score::Command* c) override { impl.submit(c); }
};
}
