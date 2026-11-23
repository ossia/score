#pragma once
#include "Record/RecordManager.hpp"
#include "Record/RecordMessagesManager.hpp"
#include "Session/RecordOptions.hpp"

#include <score/plugins/application/GUIApplicationPlugin.hpp>

#include <memory>
#include <vector>
namespace Engine
{
class ApplicationPlugin;
}
class QAction;
namespace Process
{
class Port;
}
namespace score
{
struct DocumentContext;
class Document;
}
namespace Scenario
{
class ProcessModel;
struct Point;
} // namespace Scenario
namespace Recording
{
class RecordSession;
class ApplicationPlugin final
    : public QObject
    , public score::GUIApplicationPlugin
{
public:
  ApplicationPlugin(const score::GUIApplicationContext& app);
  ~ApplicationPlugin() override;

  void record(Scenario::ProcessModel*, Scenario::Point pt);
  void recordMessages(Scenario::ProcessModel*, Scenario::Point pt);
  void stopRecord();

  //! Starts recording these ports of a document; one session at a time.
  //! Null when nothing could be recorded.
  RecordSession* startSession(
      const score::DocumentContext& ctx, const std::vector<const Process::Port*>& ports,
      RecordOptions options);
  //! Ends the session: its takes are placed
  void stopSession();
  //! A session records in this document
  bool recording(const score::DocumentContext& ctx) const noexcept;
  RecordSession* session() const noexcept { return m_session.get(); }
  //! The options of a recording started without choosing them
  RecordOptions defaultOptions() const;

  void on_closeDocument(score::Document& doc) override;

private:
  Engine::ApplicationPlugin* m_ossiaplug{};
  QAction* m_stopAction{};

  std::unique_ptr<RecordContext> m_currentContext{};
  std::unique_ptr<SingleRecorder<AutomationRecorder>> m_recManager;
  std::unique_ptr<SingleRecorder<MessageRecorder>> m_recMessagesManager;

  std::unique_ptr<RecordSession> m_session;
  const score::Document* m_sessionDocument{};
};
}
