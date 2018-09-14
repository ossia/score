#pragma once
#include <Process/LayerPresenter.hpp>

#include <QObject>
#include <QPointer>

#include <score_lib_process_export.h>
#include <wobjectdefs.h>

namespace score
{
class DocumentDelegateModel;
}
// TODO this is ugly :'(
namespace Scenario
{
class ScenarioDocumentPresenter;
}
namespace Curve
{
class Presenter;
}
// Sets the focus on a scenario document.
class SCORE_LIB_PROCESS_EXPORT FocusDispatcher : public QObject
{
  W_OBJECT(FocusDispatcher)

public:
  void focus(Scenario::ScenarioDocumentPresenter* arg_1)
  {
  }
  void focus(QPointer<Process::LayerPresenter> arg_1) E_SIGNAL(
      SCORE_LIB_PROCESS_EXPORT, focus, (QPointer<Process::LayerPresenter>),
      arg_1);
  // void focus(Curve::Presenter* arg_1) E_SIGNAL(SCORE_LIB_PROCESS_EXPORT,
  // focus, (Curve::Presenter*),arg_1);
};

Q_DECLARE_METATYPE(QPointer<Process::LayerPresenter>)
W_REGISTER_ARGTYPE(QPointer<Process::LayerPresenter>)
