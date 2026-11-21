#pragma once
#include <Process/ProcessFactory.hpp>

#include <score/plugins/UuidKey.hpp>

#include <QImage>
#include <QString>
#include <QStringList>

#include <score_plugin_library_export.h>

namespace Library
{
//! Snippets are timeline content saved as a .scenario file: the processes,
//! the boxes holding them and the cables between them. The process library
//! lists them under their own branch with this key, which no process has:
//! they are not created like a process but pasted, as their file is dropped.
inline constexpr Process::ProcessModelFactory::ConcreteKey snippetKey() noexcept
{
  return_uuid("7d4f3a2e-5b9c-4e61-a8d0-2c7e9f1b3a64");
}

//! The folder of the user library where snippets are saved.
inline constexpr const char* snippetsFolder() noexcept
{
  return "snippets";
}

inline constexpr const char* snippetSuffix() noexcept
{
  return "scenario";
}

//! What a snippet says about itself, besides its content: stored in its file
//! under the "Snippet" key, which pasting the content ignores.
struct SnippetInfo
{
  QString description;
  QStringList tags;
  QImage thumbnail;
};

//! The information of a snippet file's content; empty for a file saved
//! without any, such as one from before snippets had them.
SCORE_PLUGIN_LIBRARY_EXPORT
SnippetInfo readSnippetInfo(const QByteArray& json);

SCORE_PLUGIN_LIBRARY_EXPORT
SnippetInfo readSnippetInfoFile(const QString& path);

//! The snippet content `json` with `info` added to it.
SCORE_PLUGIN_LIBRARY_EXPORT
QByteArray withSnippetInfo(const QByteArray& json, const SnippetInfo& info);

//! What the library's search matches of a snippet besides its name.
SCORE_PLUGIN_LIBRARY_EXPORT
QString snippetSearchText(const SnippetInfo& info);
}
