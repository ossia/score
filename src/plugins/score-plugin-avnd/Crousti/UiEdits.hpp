#pragma once
#include <Process/Commands/SetControlValue.hpp>
#include <Process/Dataflow/Port.hpp>

#include <score/document/DocumentContext.hpp>

#include <QObject>
#include <QPointer>

#include <score_plugin_avnd_export.h>

#include <vector>

namespace score
{
class CommandStack;
}

namespace oscr
{
/**
 * @brief The edits a custom ui makes to its process's controls
 *
 * Through `set`, and through the transactions of its custom controls. Each
 * edit applies at once. The edits of one gesture are one undo step: they are
 * pushed together once the gesture is over. A transaction says when that is,
 * from its start to its commit. Otherwise it is the end of the event that made
 * them, or the mouse release while a button is held. An edit of a control
 * already edited in the gesture replaces it. Until they are pushed, the
 * document counts as modified.
 *
 * Anything else changing the history, a command, an undo or a redo, pushes
 * them first, so that they come before it. So does the deletion of a port
 * they edit.
 *
 * A ui reacting to a change, in on_control_update, makes no edit: the change
 * may come from execution, or be an undo or a redo, and neither edits the
 * document.
 */
class SCORE_PLUGIN_AVND_EXPORT UiEdits final : public QObject
{
public:
  UiEdits(const score::DocumentContext& ctx, QObject* parent);
  ~UiEdits() override;

  void set(Process::ControlInlet& port, ossia::value v);
  void set(Process::ControlOutlet& port, ossia::value v);

  //! A transaction: its edits are pushed at endGesture, not before
  void beginGesture();
  void endGesture();
  //! Takes back the pending edits, without an undo step
  void rollback();

  //! Pushes the pending edits as one undo step, if there are any
  void commit();

  bool pending() const noexcept { return !m_cmds.empty(); }

  //! While one is alive, the ui is reacting to a change and set does nothing
  struct Reacting
  {
    explicit Reacting(UiEdits* e) noexcept
        : edits{e}
    {
      if(edits)
        ++edits->m_reacting;
    }
    ~Reacting()
    {
      if(edits)
        --edits->m_reacting;
    }
    Reacting(const Reacting&) = delete;
    Reacting& operator=(const Reacting&) = delete;
    UiEdits* edits{};
  };

private:
  template <typename Command, typename Port>
  void edit(Port& port, ossia::value v);
  void open();
  void close();
  void scheduleCommit();
  void stopWaitingForRelease();
  bool eventFilter(QObject* watched, QEvent* event) override;

  struct Edit
  {
    const QObject* port{};
    score::Command* cmd{};
    QMetaObject::Connection destroyed;
  };

  const score::DocumentContext& m_ctx;
  QPointer<score::CommandStack> m_stack;
  std::vector<Edit> m_cmds;
  QMetaObject::Connection m_aboutToChange;
  int m_reacting{};
  int m_gestures{};
  bool m_open{};
  bool m_scheduled{};
  bool m_waitingForRelease{};
};
}
