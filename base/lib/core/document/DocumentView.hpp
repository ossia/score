#pragma once
#include <QWidget>

namespace iscore
{
    class DocumentDelegateViewInterface;
    class DocumentDelegateFactoryInterface;
    class PanelView;
    class Document;

    /**
     * @brief The DocumentView class shows a document.
     *
     * It displays a @c{DocumentDelegateViewInterface}, in
     * the central widget.
     */
    class DocumentView : public QWidget
    {
        public:
            DocumentView(DocumentDelegateFactoryInterface* viewDelegate,
                         Document* doc,
                         QWidget* parent);

            DocumentDelegateViewInterface* viewDelegate() const
            { return m_view; }

            void addPanel(PanelView*);

            Document* document() const
            {return m_document;}

        private:
            Document* m_document{};
            DocumentDelegateViewInterface* m_view {};
    };
}
