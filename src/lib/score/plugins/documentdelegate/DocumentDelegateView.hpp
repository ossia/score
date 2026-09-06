#pragma once
#include <QObject>

#include <score_lib_base_export.h>
class QWidget;

namespace score
{
class BackgroundRenderer;
class SCORE_LIB_BASE_EXPORT DocumentDelegateView : public QObject
{
public:
  using QObject::QObject;
  virtual ~DocumentDelegateView();

  virtual QWidget* getWidget() = 0;

  virtual void ready() = 0;

  //! What currently paints behind the document (a Background device, a
  //! texture port being watched...), for other views to show the same thing.
  virtual BackgroundRenderer* activeBackgroundRenderer() const noexcept;
};
}
