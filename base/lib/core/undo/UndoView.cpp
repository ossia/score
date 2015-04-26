#include "UndoView.hpp"
#include <iscore/plugins/panel/PanelModelInterface.hpp>
#include <iscore/plugins/panel/PanelPresenterInterface.hpp>
#include <iscore/plugins/panel/PanelViewInterface.hpp>

#include <core/document/DocumentPresenter.hpp>
#include <core/document/DocumentModel.hpp>

#include <QUndoView>
#include <QVBoxLayout>
#include "UndoListWidget.hpp"



class UndoPanelView : public iscore::PanelViewInterface
{
    public:
        UndoPanelView(QObject* v):
            iscore::PanelViewInterface{v},
            m_widget{new QWidget}
        {
            setObjectName(tr("Undo / Redo"));
            m_widget->setLayout(new QVBoxLayout);
        }

        QWidget* getWidget() override
        { return m_widget; }

        Qt::DockWidgetArea defaultDock() const override
        { return Qt::LeftDockWidgetArea; }

        void setStack(iscore::CommandStack* s)
        {
            delete m_list;

            m_list = new iscore::UndoListWidget{s};
            m_widget->layout()->addWidget(m_list);
        }

    private:
        iscore::UndoListWidget* m_list{};
        QWidget* m_widget{};
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

        QString modelObjectName() const override
        {
            return "UndoPanelModel";
        }

        void on_modelChanged() override
        {
            auto doc = static_cast<iscore::Document*>(model()->parent()->parent());
            static_cast<UndoPanelView*>(view())->setStack(&doc->commandStack());
        }

};

#include <core/view/View.hpp>
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
