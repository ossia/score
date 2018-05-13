#pragma once

#include <QListWidget>
#include <wobjectdefs.h>

namespace score
{
class CommandStack;

class UndoListWidget final : public QListWidget
{
  W_OBJECT(UndoListWidget)
public:
  explicit UndoListWidget(score::CommandStack& s);
  ~UndoListWidget() override;

public:
  void on_stackChanged(); W_SLOT(on_stackChanged);

private:
  score::CommandStack& m_stack;
};
}
