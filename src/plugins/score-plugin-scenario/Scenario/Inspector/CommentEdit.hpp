#pragma once
#include <QTextEdit>

#include <verdigris>

namespace Scenario
{
class CommentEdit final : public QTextEdit
{
  W_OBJECT(CommentEdit)
public:
  CommentEdit(const QString& comment, QWidget* parent)
      : QTextEdit{comment, parent}
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
