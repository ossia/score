#pragma once

#include <QListWidget>

namespace score
{
class CommandStack;

class UndoListWidget final : public QListWidget
{
  Q_OBJECT
public:
  explicit UndoListWidget(score::CommandStack& s);
  ~UndoListWidget();

public slots:
  void on_stackChanged();

private:
  score::CommandStack& m_stack;
};
}
