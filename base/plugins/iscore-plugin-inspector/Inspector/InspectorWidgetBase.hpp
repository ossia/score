#pragma once

#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <QColor>
#include <qnamespace.h>
#include <QString>
#include <QWidget>
#include <list>
#include <memory>
#include <iscore_lib_inspector_export.h>

class IdentifiedObjectAbstract;
class QVBoxLayout;

namespace iscore
{
    struct DocumentContext;
    class SelectionDispatcher;
}

/*!
 * \brief The InspectorWidgetBase class
 * Set the global structuration for an inspected element. Inherited by class that implement specific type
 *
 * Manage sections added by user.
 */
class ISCORE_LIB_INSPECTOR_EXPORT InspectorWidgetBase : public QWidget
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
                const iscore::DocumentContext& context,
                QWidget* parent);
        ~InspectorWidgetBase();

        // By default returns the name of the object.
        virtual QString tabName();

        const iscore::DocumentContext& context()
        { return m_context; }

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
        const iscore::DocumentContext& m_context;
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

