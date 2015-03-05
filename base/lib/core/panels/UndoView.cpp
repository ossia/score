#include "UndoView.hpp"
#include <interface/panel/PanelModelInterface.hpp>
#include <interface/panel/PanelPresenterInterface.hpp>
#include <interface/panel/PanelViewInterface.hpp>

#include <core/interface/document/DocumentInterface.hpp>
#include <core/document/DocumentPresenter.hpp>
#include <core/document/DocumentModel.hpp>

#include <QUndoView>

class UndoPanelView : public iscore::PanelViewInterface
{
    public:
        UndoPanelView(iscore::View* v):
            m_view{new QUndoView}
        {
            setObjectName(tr("Undo / Redo"));
        }

        QWidget* getWidget() override
        { return m_view; }

        Qt::DockWidgetArea defaultDock() const override
        { return Qt::LeftDockWidgetArea; }

    public slots:
        void setStack(QUndoStack* s)
        { m_view->setStack(s); }

    private:
        QUndoView* m_view{};
};

class UndoPanelModel : public iscore::PanelModelInterface
{
    public:
        UndoPanelModel(iscore::DocumentModel* model):
            iscore::PanelModelInterface{"UndoPanelModel", model}
        {

        }

};

class UndoPanelPresenter : public iscore::PanelPresenterInterface
{
    public:
        UndoPanelPresenter(iscore::Presenter* parent_presenter,
                           iscore::PanelViewInterface* view):
            iscore::PanelPresenterInterface{parent_presenter, view}
        {

        }

    public:
        QString modelObjectName() const
        {
            return "UndoPanelModel";
        }

        void on_modelChanged()
        {
            using namespace iscore;
            auto doc = IDocument::documentFromObject(m_model);
            // TODO put the commandQueue getter directly in IDocument.
            //static_cast<UndoPanelView*>(m_view)->setStack(doc->presenter()->commandQueue());
        }
};

iscore::PanelViewInterface*UndoPanelFactory::makeView(iscore::View* v)
{
    return new UndoPanelView{v};
}

iscore::PanelPresenterInterface*UndoPanelFactory::makePresenter(iscore::Presenter* parent_presenter,
                                                                iscore::PanelViewInterface* view)
{
    return new UndoPanelPresenter{parent_presenter, view};
}

iscore::PanelModelInterface*UndoPanelFactory::makeModel(iscore::DocumentModel* m)
{
    return new UndoPanelModel{m};
}
