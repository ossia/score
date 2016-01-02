#pragma once
#include <QObject>
#include <iscore_lib_process_export.h>

namespace Process { class LayerPresenter; }

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
        void focus(Process::LayerPresenter* obj);

    private:
        iscore::DocumentDelegateModelInterface& m_baseElementModel;
};
