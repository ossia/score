#pragma once
#include <QObject>
#include <QPointer>
#include <Process/LayerPresenter.hpp>
#include <iscore_lib_process_export.h>

namespace iscore
{
    class Document;
    class DocumentDelegateModelInterface;
}
namespace Scenario
{
class ScenarioDocumentPresenter;
}
// Sets the focus on a scenario document.
class ISCORE_LIB_PROCESS_EXPORT FocusDispatcher : public QObject
{
        Q_OBJECT

    signals:
        void focus(Scenario::ScenarioDocumentPresenter*);
        void focus(QPointer<Process::LayerPresenter>);
};

Q_DECLARE_METATYPE(QPointer<Process::LayerPresenter>)
