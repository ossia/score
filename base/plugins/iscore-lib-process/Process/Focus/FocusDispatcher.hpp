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
// Sets the focus on a scenario document.
class ISCORE_LIB_PROCESS_EXPORT FocusDispatcher : public QObject
{
        Q_OBJECT
    public:
        explicit FocusDispatcher(iscore::Document& doc);

    signals:
        void focus(QPointer<Process::LayerPresenter>);

    private:
        iscore::DocumentDelegateModelInterface& m_baseElementModel;
};

Q_DECLARE_METATYPE(QPointer<Process::LayerPresenter>)
