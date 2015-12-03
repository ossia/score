#pragma once
#include <QObject>

namespace iscore {

}  // namespace iscore

namespace iscore
{
    class PanelModel;
    class PanelView;

    class PanelPresenter : public QObject
    {
            Q_OBJECT
        public:
            PanelPresenter(PanelView* view, QObject* parent);

            virtual ~PanelPresenter();

            virtual int panelId() const = 0;

            void setModel(PanelModel* model);
            PanelModel* model() const;

        protected:
            virtual void on_modelChanged() = 0;

            PanelView* view() const;

        private:
            PanelModel* m_model{};
            PanelView* m_view{};
    };
}
