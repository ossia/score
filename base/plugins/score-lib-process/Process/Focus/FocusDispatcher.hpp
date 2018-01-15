#pragma once
#include <Process/LayerPresenter.hpp>
#include <QObject>
#include <QPointer>
#include <score_lib_process_export.h>

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
  Q_OBJECT

Q_SIGNALS:
  void focus(Scenario::ScenarioDocumentPresenter*);
  void focus(QPointer<Process::LayerPresenter>);
  void focus(Curve::Presenter*);
};

Q_DECLARE_METATYPE(QPointer<Process::LayerPresenter>)
