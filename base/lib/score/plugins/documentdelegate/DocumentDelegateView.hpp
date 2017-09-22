#pragma once
#include <QObject>
#include <score_lib_base_export.h>
class QWidget;

namespace score
{
class SCORE_LIB_BASE_EXPORT DocumentDelegateView : public QObject
{
public:
  using QObject::QObject;
  virtual ~DocumentDelegateView();

  virtual QWidget* getWidget() = 0;
};
}
