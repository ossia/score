#include "UndoView.hpp"
#include <plugin_interface/panel/PanelModelInterface.hpp>
#include <plugin_interface/panel/PanelPresenterInterface.hpp>
#include <plugin_interface/panel/PanelViewInterface.hpp>

#include <public_interface/document/DocumentInterface.hpp>
#include <core/document/DocumentPresenter.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/document/Document.hpp>

#include <QUndoView>
#include <QVBoxLayout>
#include <QListWidget>

class UndoPanelView : public iscore::PanelViewInterface
{
    public:
        UndoPanelView(iscore::View* v):
            m_widget{new QWidget}
        {
            setObjectName(tr("Undo / Redo"));
            m_widget->setLayout(new QVBoxLayout);
        }

        QWidget* getWidget() override
        { return m_widget; }

        Qt::DockWidgetArea defaultDock() const override
        { return Qt::LeftDockWidgetArea; }

    public slots:
        void setStack(iscore::CommandStack* s)
        {
            delete m_list;
            m_list = new QListWidget;
            connect(s, &iscore::CommandStack::stackChanged,
                    [=] () { on_stackChanged(s); });
            connect(m_list, &QListWidget::currentRowChanged,
                    s, &iscore::CommandStack::setIndex);

            m_widget->layout()->addWidget(m_list);

        }

    private:
        void on_stackChanged(iscore::CommandStack* s)
        {
            m_list->clear();
            m_list->addItem("<Clean state>");
            for(int i = 0; i < s->size(); i++)
            {
                m_list->addItem(s->command(i)->name());
            }
            m_list->item(s->currentIndex())->setSelected(true);
        }

        QListWidget* m_list{};
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

        QString modelObjectName() const
        {
            return "UndoPanelModel";
        }

        void on_modelChanged() override
        {
            auto doc = static_cast<iscore::Document*>(model()->parent()->parent());
            static_cast<UndoPanelView*>(view())->setStack(&doc->commandStack());
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
