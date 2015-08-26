#pragma once
#include <QObject>
class LayerPresenter;
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
        explicit FocusDispatcher(iscore::Document& doc);

    signals:
        void focus(LayerPresenter* obj);

    private:
        iscore::DocumentDelegateModelInterface& m_baseElementModel;
};
