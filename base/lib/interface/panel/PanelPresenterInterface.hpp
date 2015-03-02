#pragma once
#include <QObject>
#include <core/presenter/Presenter.hpp>
namespace iscore
{
    class PanelModelInterface;
    class PanelViewInterface;
    class SerializableCommand;

    class PanelPresenterInterface : public QObject
    {
            Q_OBJECT
        public:

            PanelPresenterInterface(Presenter* parent_presenter,
                                    PanelViewInterface* view) :
                QObject {parent_presenter},
                m_view {view},
                m_parentPresenter {parent_presenter}
            {

            }

            // The name of the model.
            virtual QString modelObjectName() const = 0;

            void setModel(PanelModelInterface* model)
            {
                m_model = model;
                on_modelChanged();
            }

            PanelModelInterface* model() const
            {
                return m_model;
            }

            virtual void on_modelChanged() = 0;

            virtual ~PanelPresenterInterface() = default;

        signals:
            void submitCommand(SerializableCommand* cmd);

        protected:
            PanelModelInterface* m_model {};
            PanelViewInterface* m_view {};
            Presenter* m_parentPresenter {};
    };
}
