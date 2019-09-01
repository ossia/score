#pragma once

#include <QListWidget>

#include <verdigris>

namespace score
{
class CommandStack;

class UndoListWidget final : public QListWidget
{
public:
  explicit UndoListWidget(score::CommandStack& s);
  ~UndoListWidget() override;

public:
  void on_stackChanged();

private:
  score::CommandStack& m_stack;
};
}
