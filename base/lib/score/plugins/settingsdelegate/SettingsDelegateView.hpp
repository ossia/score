#pragma once
#include <QWidget>
#include <score_lib_base_export.h>

namespace score
{
class SettingsDelegatePresenter;

class SCORE_LIB_BASE_EXPORT SettingsDelegateView : public QObject
{
public:
  using QObject::QObject;
  virtual ~SettingsDelegateView();
  virtual void setPresenter(SettingsDelegatePresenter* presenter)
  {
    m_presenter = presenter;
  }

  SettingsDelegatePresenter* getPresenter()
  {
    return m_presenter;
  }

  virtual QWidget* getWidget()
      = 0; // QML? ownership transfer ? ? ? what about "this" case ?

protected:
  SettingsDelegatePresenter* m_presenter{};
};
}
