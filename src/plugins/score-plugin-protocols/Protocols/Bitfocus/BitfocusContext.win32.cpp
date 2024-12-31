#include "BitfocusContext.hpp"

namespace bitfocus
{

module_handler_base::module_handler_base(QString module_path)
{
  // https://doc.qt.io/qt-6/qwineventnotifier.html
  // https://forum.qt.io/topic/146343/qsocketnotifier-with-win32-namedpipes/9
  // Or maybe QLocalSocket just works on windows?

  // FIXME
}
void module_handler_base::do_write(std::string_view res)
{
  // FIXME
}

void module_handler_base::do_write(const QByteArray& res)
{
  // FIXME
}

}
