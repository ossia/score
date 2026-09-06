#pragma once
#include <score/tools/File.hpp>

#include <QFileDialog>
#include <QString>
#include <QStringList>

#include <score_lib_base_export.h>

#include <utility>

class QWidget;

namespace score
{
/**
 * @brief Ask the user for files to bring into the document.
 *
 * Callback-shaped rather than returning a path, because not every platform can
 * answer immediately: the browser has no synchronous dialog and no filesystem
 * to name, so the bytes arrive later and have to be written somewhere the rest
 * of score can open by path. Callers that returned a path directly did not work
 * there at all.
 *
 * `onPicked(const QString& path)` is called once per chosen file, with a path
 * that can be opened. It is not called if the user cancels.
 *
 * `startDir` is where the picker opens; see score::pickerStartFolder. It has no
 * meaning on a platform with no filesystem to point at, and is ignored there.
 */
template <typename F>
void openFileToImport(
    const QString& filters, const QString& startDir, F onPicked,
    QWidget* parent = nullptr)
{
#if defined(__EMSCRIPTEN__)
  QFileDialog::getOpenFileContent(
      filters,
      [onPicked = std::move(onPicked)](
          const QString& name, const QByteArray& data) mutable {
    if(name.isEmpty() || data.isEmpty())
      return;
    if(QString staged = score::stageImportedFile(name, data); !staged.isEmpty())
      onPicked(staged);
  });
#else
  const QString fn
      = QFileDialog::getOpenFileName(
          parent, QObject::tr("Open File"), startDir, filters);
  if(!fn.isEmpty())
    onPicked(fn);
#endif
}

/**
 * @brief The same, for a set of files.
 *
 * On the browser the picker yields one file per call, so `onPicked` is invoked
 * as each arrives rather than once with the whole list.
 */
template <typename F>
void openFilesToImport(
    const QString& title, const QString& filters, const QString& startDir, F onPicked,
    QWidget* parent = nullptr)
{
#if defined(__EMSCRIPTEN__)
  openFileToImport(filters, startDir, std::move(onPicked), parent);
#else
  const QStringList files
      = QFileDialog::getOpenFileNames(parent, title, startDir, filters);
  for(const auto& f : files)
    onPicked(f);
#endif
}

/**
 * @brief Ask the user for a directory on this machine.
 *
 * Unlike the file pickers this has no meaning where there is no filesystem to
 * point at, so it reports that rather than quietly behaving as a cancellation.
 * Returns false when no directory could be asked for or the user cancelled.
 */
SCORE_LIB_BASE_EXPORT bool selectExistingDirectory(
    QWidget* parent, const QString& title, const QString& startDir, QString& out);
}
