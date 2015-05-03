#pragma once
#include <QObject>
class ProcessViewModel;
namespace iscore
{
    class Document;
    class DocumentDelegateModelInterface;
}
// Sets the focus on a scenario document.
class FocusDispatcher : public QObject
{
        Q_OBJECT
    public:
        FocusDispatcher(iscore::Document& doc);

    signals:
        void focus(const ProcessViewModel* obj);

    private:
        iscore::DocumentDelegateModelInterface& m_baseElementModel;
};
