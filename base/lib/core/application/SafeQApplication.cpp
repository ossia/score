// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "SafeQApplication.hpp"

#include <wobjectimpl.h>
W_OBJECT_IMPL(SafeQApplication)

SafeQApplication::~SafeQApplication()
{
}

#ifdef __APPLE__
bool SafeQApplication::event(QEvent* ev)
{
  bool eaten = false;
  switch (ev->type())
  {
    case QEvent::FileOpen:
    {
      auto loadString = static_cast<QFileOpenEvent*>(ev)->file();
      fileOpened(loadString);
      eaten = true;
      break;
    }
    default:
      return QApplication::event(ev);
  }
  return eaten;
}
#endif
