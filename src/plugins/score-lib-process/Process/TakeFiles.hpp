#pragma once
#include <QString>

#include <score_lib_process_export.h>

#include <functional>

namespace score
{
struct DocumentContext;
}

namespace Process
{
/**
 * Where the takes of a document are written: the Takes folder next to the
 * saved document, or, for a document never saved, a folder of the
 * application's data. Shared by the loopers and the recordings.
 */
SCORE_LIB_PROCESS_EXPORT QString takesFolder(const score::DocumentContext& ctx);

/**
 * Picks the first take number no file of `folder` has for `prefix`
 * ("<prefix>-take<N>.<ext>", or "<prefix>-take<N>-<anything>") and creates
 * the empty file `fileFor(number)`, so that no other writer gets the same
 * number: the choice and the creation are one step for every writer of every
 * document. Creates the folder when needed. Returns the number.
 */
SCORE_LIB_PROCESS_EXPORT int reserveTakeNumber(
    const QString& folder, const QString& prefix,
    const std::function<QString(int)>& fileFor);

//! "<folder>/<prefix>-take<number>.<extension>"
SCORE_LIB_PROCESS_EXPORT QString
takeFile(const QString& folder, const QString& prefix, int number, const QString& extension);
}
