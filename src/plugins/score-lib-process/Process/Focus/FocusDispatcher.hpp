#pragma once
#include <Process/LayerPresenter.hpp>

#include <QObject>
#include <QPointer>

#include <score_lib_process_export.h>

#include <verdigris>

namespace Curve
{
class Presenter;
}
// Sets the focus on a scenario document.
class SCORE_LIB_PROCESS_EXPORT FocusDispatcher : public QObject
{
  W_OBJECT(FocusDispatcher)

public:
  void focus(QPointer<Process::LayerPresenter> arg_1)
      E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, focus, (QPointer<Process::LayerPresenter>), arg_1)
};

Q_DECLARE_METATYPE(QPointer<Process::LayerPresenter>)
W_REGISTER_ARGTYPE(QPointer<Process::LayerPresenter>)
