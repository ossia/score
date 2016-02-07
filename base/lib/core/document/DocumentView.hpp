#pragma once

#include <QWidget>

namespace iscore
{
    class Document;
    class DocumentDelegateFactory;
    class DocumentDelegateViewInterface;
    class PanelView;

    /**
     * @brief The DocumentView class shows a document.
     *
     * It displays a @c{DocumentDelegateViewInterface}, in
     * the central widget.
     */
    class DocumentView final : public QWidget
    {
        public:
            DocumentView(DocumentDelegateFactory& viewDelegate,
                         const Document& doc,
                         QWidget* parent);

            DocumentDelegateViewInterface& viewDelegate() const
            { return *m_view; }

            void addPanel(PanelView*);

            const Document& document() const
            {return m_document;}

        private:
            const Document& m_document;
            DocumentDelegateViewInterface* m_view {};
    };
}
