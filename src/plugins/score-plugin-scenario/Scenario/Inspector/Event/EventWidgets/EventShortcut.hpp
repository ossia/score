#pragma once
#include <QString>
#include <QWidget>

#include <wobjectdefs.h>

class QCheckBox;
class QPushButton;

namespace Scenario
{
// TODO refactor with SelectableButton
class EventShortCut final : public QWidget
{
  W_OBJECT(EventShortCut)
public:
  EventShortCut(QString eventId, QWidget* parent = nullptr);

  bool isChecked();
  QString eventName();

public:
  void eventSelected() W_SIGNAL(eventSelected);

private:
  QCheckBox* m_box;
  QPushButton* m_eventBtn;
};
}
