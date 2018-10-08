// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "SafeQApplication.hpp"
#include <score/tools/std/Invoke.hpp>
#include <wobjectimpl.h>
W_OBJECT_IMPL(SafeQApplication)

namespace score
{
PostedEventBase::~PostedEventBase() = default;
}

SafeQApplication::~SafeQApplication()
{
}

bool SafeQApplication::event(QEvent* ev)
{
  switch (ev->type())
  {
#ifdef __APPLE__
    case QEvent::FileOpen:
    {
      auto loadString = static_cast<QFileOpenEvent*>(ev)->file();
      fileOpened(loadString);
      return true;
    }
#endif
    case score::PostedEventBase::static_type:
    {
      (*static_cast<score::PostedEventBase*>(ev))();
      return true;
    }
    default:
      return QApplication::event(ev);
  }
}
