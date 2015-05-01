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
                                    PanelViewInterface* view);

            virtual int panelId() const = 0;

            void setModel(PanelModelInterface* model);
            PanelModelInterface* model() const;

        protected:
            virtual void on_modelChanged() = 0;

            PanelViewInterface* view() const;
            Presenter* presenter() const;

        private:
            PanelModelInterface* m_model{};
            PanelViewInterface* m_view{};
            Presenter* m_parentPresenter{};
    };
}
