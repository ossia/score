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
            m_undoAction->setShortcut(QKeySequence::Undo);
            m_undoAction->setEnabled(false);
            m_undoAction->setText(tr("Undo"));
            connect(m_undoAction, &QAction::triggered,
                    [&] ()
            {
                presenter()->currentDocument()->commandStack()->undoAndNotify();
            });

            m_redoAction->setShortcut(QKeySequence::Redo);
            m_redoAction->setEnabled(false);
            m_redoAction->setText(tr("Redo"));
            connect(m_redoAction, &QAction::triggered,
                    [&] ()
            {
                presenter()->currentDocument()->commandStack()->redoAndNotify();
            });
        }

        ~UndoPanelPresenter()
        {
            for(auto& connection : m_connections)
            {
                disconnect(connection);
            }
        }

        QString modelObjectName() const
        {
            return "UndoPanelModel";
        }

        void on_modelChanged()
        {
            using namespace iscore;
            for(auto& connection : m_connections)
                disconnect(connection);

            m_connections.clear();

            m_connections.push_back(
                        connect(presenter()->currentDocument()->commandStack(), &CommandStack::canUndoChanged,
                                [&] (bool b) { m_undoAction->setEnabled(b); }));
            m_connections.push_back(
                        connect(presenter()->currentDocument()->commandStack(), &CommandStack::canRedoChanged,
                                [&] (bool b) { m_redoAction->setEnabled(b); }));

            m_connections.push_back(
                        connect(presenter()->currentDocument()->commandStack(), &CommandStack::undoTextChanged,
                                [&] (const QString& s) { m_undoAction->setText(tr("Undo ") + s);}));
            m_connections.push_back(
                        connect(presenter()->currentDocument()->commandStack(), &CommandStack::redoTextChanged,
                                [&] (const QString& s) { m_redoAction->setText(tr("Redo ") + s);}));
        }

    private:
        // Connections to keep for the running document.
        QList<QMetaObject::Connection> m_connections;

        QAction* m_undoAction{new QAction{this}};
        QAction* m_redoAction{new QAction{this}};
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
