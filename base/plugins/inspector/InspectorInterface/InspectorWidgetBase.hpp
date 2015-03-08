#pragma once

#include <QWidget>
#include <core/tools/ObjectPath.hpp>
#include <public_interface/selection/SelectionStack.hpp>
#include <core/presenter/command/OngoingCommandManager.hpp>
class QVBoxLayout;
class QLineEdit;
class QLabel;
class QTextEdit;
class QPushButton;
class InspectorSectionWidget;
class QScrollArea;

namespace iscore
{
    class SerializableCommand;
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
        explicit InspectorWidgetBase(QObject* inspectedObj = 0, QWidget* parent = 0);

    public slots:

        // Manage Sections

        /*!
         * \brief addNewSection Add an InspectorSectionWidget in the main widget
         * \param sectionName The name of this new section.
         * \param content The Widget.
         */
        void addNewSection(QString sectionName, QWidget* content = 0);

        /*!
         * \brief addSubSection Add an InspectorSectionWidget in the choose InspectorSectionWidget
         *
         * \param parentSection The name of the parent InspectorSectionWidget
         * \param subSection The name of the new InspectorSectionWidget
         * \param content The InspectorSectionWidget
         */
        void addSubSection(QString parentSection, QString subSection, InspectorSectionWidget* content);

        /*!
         * \brief insertSection Insert a InspectorSectionWidget in the main widget
         *
         * \param index Index
         * \param name The name of the new Widget
         * \param content The widget
         */
        void insertSection(int index, QString name, QWidget* content = 0);

        void removeSection(QString sectionName);  //todo
        void updateSectionsView(QVBoxLayout* layout, QVector<QWidget*>& contents);

        void addHeader(QWidget* header);

        // Manage Values
        void setInspectedObject(QObject* object);
        QObject* inspectedObject() const;

        // getters
        QVBoxLayout* areaLayout()
        {
            return _scrollAreaLayout;
        }

        ICommandDispatcher* commandDispatcher() const
        { return m_commandDispatcher; }

        iscore::SelectionDispatcher* selectionDispatcher() const
        { return m_selectionDispatcher; }

    private:
        QObject* _inspectedObject {};
        ICommandDispatcher* m_commandDispatcher{};
        iscore::SelectionDispatcher* m_selectionDispatcher{};
        QVBoxLayout* _scrollAreaLayout {};

        QVector<QWidget*> _sections {};
        QColor _currentColor {Qt::gray};


        static const int m_colorIconSize
        {
            21
        };

        QVBoxLayout* _layout {};
};
