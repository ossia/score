#include <score/widgets/FileDialog.hpp>

#include <QDebug>

namespace score
{
bool selectExistingDirectory(
    QWidget* parent, const QString& title, const QString& startDir, QString& out)
{
#if defined(__EMSCRIPTEN__)
  // There is no directory to name: the browser hands over individual files.
  // Returning an empty string would be indistinguishable from a cancellation
  // and leave the caller waiting for something that cannot arrive.
  qWarning() << "Cannot ask for a directory here:" << title;
  return false;
#else
  const QString dir = QFileDialog::getExistingDirectory(parent, title, startDir);
  if(dir.isEmpty())
    return false;
  out = dir;
  return true;
#endif
}
}
