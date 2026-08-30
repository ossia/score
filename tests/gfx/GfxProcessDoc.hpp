#pragma once

// Document-level fixture for the Gfx *process* models.
//
// The render tests in this directory build score::gfx nodes by hand; nothing
// there ever constructs a Process::ProcessModel. These helpers put a real
// document under a test and create processes through the same command the
// application uses (AddOnlyProcessToInterval), so the factory, the construction
// data and the undo stack are all the real ones.
//
// GUI (not APP): the script-edit commands save and restore cables and ports,
// and undo prunes the selection through the document presenter. Also, forcing
// the offscreen QPA platform anywhere in this directory is a trap — it has no
// GL, so any test that later grows a render assertion would silently run on
// the Null RHI backend. Everything here therefore runs on the real display.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <Process/Process.hpp>
#include <Process/ProcessList.hpp>

#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <QDir>
#include <QFile>
#include <QPointF>

#include <catch2/catch_test_macros.hpp>

namespace score::test::gfxproc
{

inline Scenario::IntervalModel& base_interval(score::Document& doc)
{
  return static_cast<Scenario::ScenarioDocumentModel&>(doc.model().modelDelegate())
      .baseInterval();
}

/// Create a process of `uuid` in the document's base interval, through
/// AddOnlyProcessToInterval so that it lands on the undo stack.
/// `data` is the factory's construction data — for the script processes that
/// is the absolute path of a shader file; empty means "default script".
inline Process::ProcessModel* add_process(
    const score::GUIApplicationContext& ctx, score::Document& doc, const QString& uuid,
    const QString& data = {})
{
  auto& interval = base_interval(doc);
  auto& factories = ctx.interfaces<Process::ProcessFactoryList>();
  const auto key = UuidKey<Process::ProcessModel>::fromString(uuid);
  auto* factory = factories.get(key);
  if(!factory)
    return nullptr;

  CommandDispatcher<> disp{doc.context().commandStack};
  disp.submit<Scenario::Command::AddOnlyProcessToInterval>(
      interval, factory->concreteKey(),
      data.isEmpty() ? factory->customConstructionData() : data, QPointF{});

  Process::ProcessModel* out{};
  for(auto& p : interval.processes)
    if(p.concreteKey() == key)
      out = &p;
  return out;
}

/// Names of the process' inlets, in order — the port surface a script change
/// rebuilds, and the thing a script-edit undo has to put back byte for byte.
inline std::vector<QString> inlet_names(const Process::ProcessModel& p)
{
  std::vector<QString> v;
  for(auto* inl : p.inlets())
    v.push_back(inl->name());
  return v;
}

inline std::vector<QString> outlet_names(const Process::ProcessModel& p)
{
  std::vector<QString> v;
  for(auto* out : p.outlets())
    v.push_back(out->name());
  return v;
}

#if defined(GFX_TEST_CORPUS_DIR)
inline QString corpus(const char* name)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR "/") + QString::fromUtf8(name);
}

inline QString corpus_text(const char* name)
{
  QFile f{corpus(name)};
  if(!f.open(QIODevice::ReadOnly))
    return {};
  return QString::fromUtf8(f.readAll());
}
#endif

/// A scratch directory under /tmp for the fixtures a test has to write
/// (images, truncated files, ...). Never inside the source tree.
inline QString scratch_dir(const char* who)
{
  const QString d = QDir::tempPath() + QStringLiteral("/score-gfx-tests/") + who;
  QDir{}.mkpath(d);
  return d;
}

}
