#pragma once

#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <qcolor.h>
#include <qnamespace.h>
#include <qstring.h>
#include <qwidget.h>
#include <list>
#include <memory>

class IdentifiedObjectAbstract;
class QVBoxLayout;

namespace iscore
{
    class Document;
    class SelectionDispatcher;
}

/*!
 * \brief The InspectorWidgetBase class
 * Set the global structuration for an inspected element. Inherited by class that implement specific type
 *
 * Manage sections added by user.
 */
class InspectorWidgetBase : public QWidget
{
        Q_OBJECT
    public:
        /*!
         * \brief InspectorWidgetBase Constructor
         * \param inspectedObj The selected object
         * \param parent The parent Widget
         */
        explicit InspectorWidgetBase(
                const IdentifiedObjectAbstract&inspectedObj,
                iscore::Document& doc,
                QWidget* parent);
        ~InspectorWidgetBase();

        // By default returns the name of the object.
        virtual QString tabName();

        iscore::Document& doc()
        { return m_document; }

        void updateSectionsView(QVBoxLayout* layout, const std::list<QWidget*>& contents);
        void updateAreaLayout(std::list<QWidget*>& contents);

        void addHeader(QWidget* header);

        // Manage Values
        const IdentifiedObjectAbstract& inspectedObject() const;
        const IdentifiedObjectAbstract* inspectedObject_addr() const
        { return &inspectedObject(); }

        // getters
        QVBoxLayout* areaLayout()
        {
            return m_scrollAreaLayout;
        }

        CommandDispatcher<>* commandDispatcher() const
        { return m_commandDispatcher; }

        iscore::SelectionDispatcher& selectionDispatcher() const
        { return *m_selectionDispatcher.get(); }

    private:
        const IdentifiedObjectAbstract& m_inspectedObject;
        iscore::Document& m_document;
        CommandDispatcher<>* m_commandDispatcher{};
        std::unique_ptr<iscore::SelectionDispatcher> m_selectionDispatcher;
        QVBoxLayout* m_scrollAreaLayout {};

        std::list<QWidget*> m_sections;
        QColor _currentColor {Qt::gray};


        static const int m_colorIconSize
        {
            21
        };

        QVBoxLayout* m_layout {};
};

