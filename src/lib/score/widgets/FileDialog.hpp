#pragma once
#include <score/document/DocumentContext.hpp>
#include <score/tools/Environment.hpp>
#include <score/tools/File.hpp>

#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
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
 */
template <typename F>
void openFileToImport(
    const score::DocumentContext& ctx, const QString& filters, F onPicked,
    QWidget* parent = nullptr)
{
#if defined(__EMSCRIPTEN__)
  QFileDialog::getOpenFileContent(
      filters,
      [&ctx, onPicked = std::move(onPicked)](
          const QString& name, const QByteArray& data) mutable {
    if(name.isEmpty() || data.isEmpty())
      return;
    if(QString imported = score::importFile(name, data, ctx.environment());
       !imported.isEmpty())
      onPicked(imported);
  });
#else
  const QString fn
      = QFileDialog::getOpenFileName(parent, QObject::tr("Open File"), {}, filters);
  // A path chosen here names nothing on the machine running the score, so when
  // that is not this one the bytes go with it.
  if(QString imported = score::importPickedFile(fn, ctx.environment());
     !imported.isEmpty())
    onPicked(imported);
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
    const score::DocumentContext& ctx, const QString& title, const QString& filters,
    F onPicked, QWidget* parent = nullptr)
{
#if defined(__EMSCRIPTEN__)
  openFileToImport(ctx, filters, std::move(onPicked), parent);
#else
  const QStringList files
      = QFileDialog::getOpenFileNames(parent, title, QString{}, filters);
  for(const auto& fn : files)
  {
    if(QString imported = score::importPickedFile(fn, ctx.environment());
       !imported.isEmpty())
      onPicked(imported);
  }
#endif
}

/**
 * @brief Ask the user for a directory on this machine.
 *
 * Unlike the file pickers this has no meaning where there is no filesystem to
 * point at, so it reports that rather than quietly behaving as a cancellation.
 * Returns false when no directory could be asked for or the user cancelled.
 */
SCORE_LIB_BASE_EXPORT bool
selectExistingDirectory(QWidget* parent, const QString& title, QString& out);
}
