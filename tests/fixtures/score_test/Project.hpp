#pragma once

// Building blocks for the tests around a document's files: a project folder on
// disk, media to put in it, and processes that reference that media.
//
// Shared rather than copied because every one of these tests needs the same
// setup -- a saved document, a folder of media beside it -- and the moment two
// copies of "make a sound process" exist they start disagreeing.

#include <score_test/Document.hpp>

#include <Process/Dataflow/WidgetInlets.hpp>
#include <Process/FileOperation.hpp>
#include <Process/ProcessList.hpp>

#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <ossia/network/value/value_conversion.hpp>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPointF>

#include <catch2/catch_test_macros.hpp>

namespace score::test
{

inline const QString sound_process_uuid
    = QStringLiteral("63174570-d608-44bf-a9cb-e6f5a11f73cc");
inline const QString obj_loader_uuid
    = QStringLiteral("5df71765-505f-4ab7-98c1-f305d10a01ef");
inline const QString video_process_uuid
    = QStringLiteral("32dc5341-7748-4c31-a226-82e6bd685744");

inline Scenario::IntervalModel& base_interval(score::Document& doc)
{
  return static_cast<Scenario::ScenarioDocumentModel&>(doc.model().modelDelegate())
      .baseInterval();
}

inline void write_file(const QString& path, const QByteArray& content)
{
  QDir{}.mkpath(QFileInfo{path}.absolutePath());
  QFile f{path};
  REQUIRE(f.open(QIODevice::WriteOnly));
  f.write(content);
}

/// A real 16-bit mono WAV of `seconds`, so the decoders do not have to be
/// stubbed out for the sound process to accept it -- and so that trimming has
/// something whose size actually changes.
inline void write_wav(const QString& path, double seconds, char sample_byte = 0x11)
{
  constexpr int rate = 44100;
  const int frames = int(seconds * rate);
  const int data_size = frames * 2;

  QByteArray out;
  out.reserve(data_size + 64);
  const auto u32 = [&](quint32 v) {
    out.append(char(v & 0xff));
    out.append(char((v >> 8) & 0xff));
    out.append(char((v >> 16) & 0xff));
    out.append(char((v >> 24) & 0xff));
  };
  const auto u16 = [&](quint16 v) {
    out.append(char(v & 0xff));
    out.append(char((v >> 8) & 0xff));
  };

  out.append("RIFF");
  u32(36 + data_size);
  out.append("WAVE");
  out.append("fmt ");
  u32(16);
  u16(1);    // PCM
  u16(1);    // mono
  u32(rate); // rate
  u32(rate * 2);
  u16(2);  // block align
  u16(16); // bits
  out.append("data");
  u32(data_size);
  out.append(QByteArray(data_size, sample_byte));

  write_file(path, out);
}

/// Add a process to the document's base interval. Returns nullptr when the
/// plug-in providing it is not built, so a test can skip that part rather than
/// claim to have covered it.
inline Process::ProcessModel*
add_process(score::Document& doc, const QString& uuid, const QString& data)
{
  const auto key = UuidKey<Process::ProcessModel>::fromString(uuid);
  auto& factories = doc.context().app.interfaces<Process::ProcessFactoryList>();
  auto* factory = factories.get(key);
  if(!factory)
    return nullptr;

  auto& interval = base_interval(doc);
  auto* cmd = new Scenario::Command::AddOnlyProcessToInterval{
      interval, factory->concreteKey(), data, QPointF{}};
  CommandDispatcher<>{doc.context().commandStack}.submit(cmd);

  auto it = interval.processes.find(cmd->processId());
  return it != interval.processes.end() ? &(*it) : nullptr;
}

/// The first file-valued control port of a process, if it has one.
inline Process::ControlInlet* file_control(Process::ProcessModel& proc)
{
  for(auto* inlet : proc.inlets())
    if(auto* file = qobject_cast<Process::FileChooserBase*>(inlet))
      return file;
  return nullptr;
}

inline QString control_string(const Process::ControlInlet& inlet)
{
  return QString::fromStdString(ossia::convert<std::string>(inlet.value()));
}

/// The report entry whose stored path contains `needle`.
inline const Process::FileEntry*
entry_for(const Process::FileReport& r, const QString& needle)
{
  for(const auto& e : r.entries)
    if(e.storedPath.contains(needle))
      return &e;
  return nullptr;
}

/// Canonical path of a temporary directory: QTemporaryDir hands out a path
/// under /tmp, which is a symlink on some systems.
inline QString canonical(const QString& path)
{
  return QFileInfo{path}.canonicalFilePath();
}

/// A document saved into `folder`, ready for anything project-relative.
inline score::Document* project_document(
    const score::GUIApplicationContext& ctx, const QString& folder,
    const QString& name = QStringLiteral("project.score"))
{
  auto* doc = new_document(ctx);
  REQUIRE(doc != nullptr);
  REQUIRE(ctx.docManager.saveDocumentAs(*doc, folder + '/' + name));
  REQUIRE(doc->metadata().projectFolder() == canonical(folder));
  return doc;
}
}
