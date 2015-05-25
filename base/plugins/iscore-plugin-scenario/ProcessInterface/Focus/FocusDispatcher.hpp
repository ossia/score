#pragma once
#include <QObject>
class ProcessPresenter;
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
        void focus(ProcessPresenter* obj);

    private:
        iscore::DocumentDelegateModelInterface& m_baseElementModel;
};
