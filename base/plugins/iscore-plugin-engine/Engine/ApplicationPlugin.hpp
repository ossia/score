#pragma once

#include <Process/TimeValue.hpp>
#include <QString>
#include <iscore/plugins/application/GUIApplicationPlugin.hpp>
#include <memory>

#include <Engine/Executor/ContextMenu/PlayContextMenu.hpp>
#include <iscore/plugins/documentdelegate/plugin/DocumentPlugin.hpp>
#include <iscore_plugin_engine_export.h>
namespace iscore
{

class Document;
} // namespace iscore
struct VisitorVariant;

namespace Scenario
{
class IntervalModel;
}
namespace Engine
{
namespace LocalTree { class DocumentPlugin; }
namespace Execution
{
class ClockManager;
struct Context;
class IntervalComponent;
}
}
// TODO this should have "OSSIA Policies" : one would be the
// "basic" that corresponds to the default scenario.
// One would be the "distributed" policy which provides the
// same functionalities but for scenario executing on different computers.

namespace Engine
{
/*
class CoreApplicationPlugin : public iscore::ApplicationPlugin
{
public:
  CoreApplicationPlugin(const iscore::ApplicationContext& ctx);

};
*/

class ISCORE_PLUGIN_ENGINE_EXPORT ApplicationPlugin final
    : public QObject,
      public iscore::GUIApplicationPlugin
{
public:
  ApplicationPlugin(const iscore::GUIApplicationContext& app);
  ~ApplicationPlugin();

  bool handleStartup() override;

  void on_initDocument(iscore::Document& doc) override;
  void on_createdDocument(iscore::Document& doc) override;

  void on_documentChanged(
      iscore::Document* olddoc, iscore::Document* newdoc) override;

  void prepareNewDocument() override;

  void on_play(bool, ::TimeVal t = ::TimeVal::zero());
  void on_play(
      Scenario::IntervalModel&,
      bool,
      std::function<void(const Engine::Execution::Context&)> setup = {},
      ::TimeVal t = ::TimeVal::zero());

  void on_record(::TimeVal t);

  bool playing() const
  {
    return m_playing;
  }
  bool paused() const
  {
    return m_paused;
  }

  void on_stop();
private:
  void on_init();

  void initLocalTreeNodes(Engine::LocalTree::DocumentPlugin&);

  std::unique_ptr<Engine::Execution::ClockManager>
  makeClock(const Engine::Execution::Context&);

  Engine::Execution::PlayContextMenu m_playActions;

  std::unique_ptr<Engine::Execution::ClockManager> m_clock;
  bool m_playing{false}, m_paused{false};
};
}
