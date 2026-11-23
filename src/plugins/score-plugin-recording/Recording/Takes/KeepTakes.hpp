#pragma once
#include <QString>

#include <score_plugin_recording_export.h>

namespace score
{
class Document;
}

namespace Recording
{
/**
 * A document that was never saved records its takes outside of any project
 * (Process::takesFolder). Saved to `newFileName` for the first time, they are
 * copied into the Takes folder next to it, and the references to them point
 * there in one undoable command. The first copies stay where
 * they were, for what undo brings back. A name already taken in Takes gets a
 * suffix. Returns how many files were copied.
 */
SCORE_PLUGIN_RECORDING_EXPORT
int keepTakesWithProject(score::Document& doc, const QString& newFileName);
}
