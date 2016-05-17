#pragma once

#include <QMainWindow>
#include <QPair>
#include <QString>
#include <vector>
#include <core/presenter/Action.hpp>

class QCloseEvent;
class QDockWidget;
class QEvent;
class QObject;
class QTabWidget;

#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore_lib_base_export.h>

namespace iscore
{
    class DocumentModel;
    class DocumentView;
    class PanelView;
    class PanelDelegate;
    class Presenter;

    /**
     * @brief The View class
     *
     * The main display of the application.
     */
    class ISCORE_LIB_BASE_EXPORT View final : public QMainWindow
    {
            Q_OBJECT
        public:
            View(QObject* parent);

            void setPresenter(Presenter*);

            void addDocumentView(iscore::DocumentView*);
            void setupPanelView(PanelView* v);
            void setupPanel(PanelDelegate* v);

            void closeDocument(iscore::DocumentView* doc);

            void closeEvent(QCloseEvent*) override;

        signals:
            /**
             * @brief insertActionIntoMenubar
             *
             * A quick signal to add an action.
             * Especially considering that we already know the presenter.
             */
            void insertActionIntoMenubar(PositionedMenuAction);

            void activeDocumentChanged(const Id<DocumentModel>&);
            void closeRequested(const Id<DocumentModel>&);

            void activeWindowChanged();

        public slots:
            void on_fileNameChanged(DocumentView* d, const QString& newName);

        private:
            void changeEvent(QEvent *) override;

            std::vector<QPair<PanelView*, QDockWidget*>> m_leftWidgets;
            std::vector<QPair<PanelView*, QDockWidget*>> m_rightWidgets;

            std::vector<QPair<PanelDelegate*, QDockWidget*>> m_leftPanels;
            std::vector<QPair<PanelDelegate*, QDockWidget*>> m_rightPanels;

            Presenter* m_presenter{};
            QTabWidget* m_tabWidget{};
    };
}
