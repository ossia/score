#pragma once
#include <QWidget>

namespace iscore
{
    class DocumentDelegateViewInterface;
    class DocumentDelegateFactoryInterface;
    class PanelViewInterface;
    class Document;

    /**
     * @brief The DocumentView class is the central view of i-score.
     *
     * It displays a @c{DocumentDelegateViewInterface}.
     */
    class DocumentView : public QWidget
    {
        public:
            DocumentView(DocumentDelegateFactoryInterface* viewDelegate,
                         Document* doc,
                         QWidget* parent);

            DocumentDelegateViewInterface* viewDelegate() const
            { return m_view; }

            void addPanel(PanelViewInterface*);

            Document* document() const
            {return m_document;}

        private:
            Document* m_document{};
            DocumentDelegateViewInterface* m_view {};
    };
}
