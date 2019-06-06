#pragma once
#include <QTextEdit>

#include <verdigris>

namespace Scenario
{
class CommentEdit final : public QTextEdit
{
  W_OBJECT(CommentEdit)
public:
  template <typename... Args>
  CommentEdit(Args&&... args) : QTextEdit{std::forward<Args>(args)...}
  {
    setMouseTracking(true);
  }

  void leaveEvent(QEvent* ev) override
  {
    QTextEdit::leaveEvent(ev);
    editingFinished();
  }

public:
  void editingFinished() W_SIGNAL(editingFinished);
};
}
