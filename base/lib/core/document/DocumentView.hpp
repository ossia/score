#pragma once
#include <QWidget>

namespace iscore
{
    class DocumentDelegateViewInterface;
    class DocumentDelegateFactoryInterface;
    class PanelViewInterface;

    /**
     * @brief The DocumentView class is the central view of i-score.
     *
     * It displays a @c{DocumentDelegateViewInterface}.
     */
    class DocumentView : public QWidget
    {
        public:
            DocumentView(DocumentDelegateFactoryInterface* viewDelegate, QWidget* parent);

            DocumentDelegateViewInterface* viewDelegate() const
            { return m_view; }

            void addPanel(PanelViewInterface*);

        private:
            DocumentDelegateViewInterface* m_view {};
    };
}
