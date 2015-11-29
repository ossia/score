#pragma once
#include <qobject.h>

namespace iscore {
class Presenter;
}  // namespace iscore

namespace iscore
{
    class PanelModel;
    class PanelView;

    class PanelPresenter : public QObject
    {
            Q_OBJECT
        public:
            PanelPresenter(Presenter* parent_presenter,
                                    PanelView* view);

            virtual ~PanelPresenter();

            virtual int panelId() const = 0;

            void setModel(PanelModel* model);
            PanelModel* model() const;

        protected:
            virtual void on_modelChanged() = 0;

            PanelView* view() const;
            Presenter* presenter() const;

        private:
            PanelModel* m_model{};
            PanelView* m_view{};
            Presenter* m_parentPresenter{};
    };
}
