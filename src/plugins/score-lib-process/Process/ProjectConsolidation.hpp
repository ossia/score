#pragma once
#include <Process/FileOperation.hpp>

#include <score/tools/ProjectFiles.hpp>

#include <QString>

#include <score_lib_process_export.h>

namespace score
{
struct DocumentContext;
}

namespace Process
{

/**
 * @brief Plan the consolidation of a document into its own folder.
 *
 * Pure: reads the document and the filesystem, writes nothing. The result is
 * what the UI shows the user before committing.
 *
 * `projectFolder` defaults to the folder holding the document. Passing a
 * different one is how "save a copy into a new project folder" works: the
 * references still resolve against the document's current location, while the
 * destinations are computed for the new one.
 */
SCORE_LIB_PROCESS_EXPORT
FileReport analyzeProjectFiles(
    const score::DocumentContext& ctx, const score::ConsolidateOptions& opts,
    QString projectFolder = {});

/**
 * @brief Perform the consolidation planned by analyzeProjectFiles.
 *
 * Copies the files, then rewrites the document in a single undoable command.
 * Originals are never moved or deleted, and an existing destination is never
 * overwritten.
 *
 * The returned report describes what actually happened, entry per entry: a
 * copy that failed comes back as FileAction::Failed with its reason, and its
 * reference is left untouched.
 */
SCORE_LIB_PROCESS_EXPORT
FileReport consolidateProjectFiles(
    const score::DocumentContext& ctx, const score::ConsolidateOptions& opts,
    QString projectFolder = {});

//! How many references the document resolves through its own folder.
//! These are exactly the ones that stop working when the document is saved
//! somewhere else without its media.
SCORE_LIB_PROCESS_EXPORT
int countProjectRelativeFiles(const score::DocumentContext& ctx);

/**
 * @brief Turn every project-relative reference into the absolute file it
 *        currently points at.
 *
 * The counterpart of consolidating when a document is saved into another
 * folder: rather than let "<PROJECT>:Audio/kick.wav" quietly start meaning a
 * file that is not there, pin it to the one that is. The result is a document
 * that still plays, at the cost of no longer being self-contained.
 *
 * Returns the number of references that were re-anchored. The change goes on
 * the command stack like any other.
 */
SCORE_LIB_PROCESS_EXPORT
int reanchorProjectFiles(const score::DocumentContext& ctx);

//! The folder a consolidation would target, and the roots the rewritten
//! references would mean. Shared by everything that writes into the project.
struct SCORE_LIB_PROCESS_EXPORT ProjectTarget
{
  //! Canonical (or lexical, when it does not exist yet) project folder.
  QString folder;
  //! Where references resolve from today.
  score::PathRoots sourceRoots;
  //! What a rewritten reference will mean once the document lives in `folder`.
  score::PathRoots destinationRoots;
};

SCORE_LIB_PROCESS_EXPORT
ProjectTarget
projectTarget(const score::DocumentContext& ctx, QString projectFolder = {});
}
