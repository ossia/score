#pragma once

#include <Process/TimeValue.hpp>
#include <QString>
#include <score/plugins/application/GUIApplicationPlugin.hpp>
#include <memory>

#include <Engine/Executor/ContextMenu/PlayContextMenu.hpp>
#include <score/plugins/documentdelegate/plugin/DocumentPlugin.hpp>
#include <score_plugin_engine_export.h>
namespace score
{

class Document;
} // namespace score
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
class CoreApplicationPlugin : public score::ApplicationPlugin
{
public:
  CoreApplicationPlugin(const score::ApplicationContext& ctx);

};
*/

class SCORE_PLUGIN_ENGINE_EXPORT ApplicationPlugin final
    : public QObject,
      public score::GUIApplicationPlugin
{
public:
  ApplicationPlugin(const score::GUIApplicationContext& app);
  ~ApplicationPlugin();

  bool handleStartup() override;
  score::GUIElements makeGUIElements() override;

  void on_initDocument(score::Document& doc) override;
  void on_createdDocument(score::Document& doc) override;

  void on_documentChanged(
      score::Document* olddoc, score::Document* newdoc) override;

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
