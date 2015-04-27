#pragma once

#include <QWidget>
#include <iscore/tools/ObjectPath.hpp>
#include <iscore/selection/SelectionDispatcher.hpp>
#include <iscore/command/OngoingCommandManager.hpp>
#include <QVBoxLayout>
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
        explicit InspectorWidgetBase(QObject* inspectedObj, QWidget* parent);
        ~InspectorWidgetBase();

    public slots:
        void updateSectionsView(QVBoxLayout* layout, QVector<QWidget*>& contents);
        void updateAreaLayout(QVector<QWidget*>& contents);

        void addHeader(QWidget* header);

        // Manage Values
        void setInspectedObject(QObject* object);
        QObject* inspectedObject() const;

        // getters
        QVBoxLayout* areaLayout()
        {
            return m_scrollAreaLayout;
        }

        CommandDispatcher<>* commandDispatcher() const
        { return m_commandDispatcher; }

        iscore::SelectionDispatcher* selectionDispatcher() const
        { return m_selectionDispatcher.get(); }

    private:
        QObject* _inspectedObject {};
        CommandDispatcher<>* m_commandDispatcher{};
        std::unique_ptr<iscore::SelectionDispatcher> m_selectionDispatcher;
        QVBoxLayout* m_scrollAreaLayout {};

        QVector<QWidget*> _sections {};
        QColor _currentColor {Qt::gray};


        static const int m_colorIconSize
        {
            21
        };

        QVBoxLayout* _layout {};
};

#include <QFrame>
class Separator : public QWidget
{
    public:
        Separator(QWidget* parent) :
            QWidget {parent}
        {
            this->setLayout(new QVBoxLayout);
            QFrame* f = new QFrame;
            this->layout()->addWidget(f);

            f->setFrameShape(QFrame::HLine);
            f->setLineWidth(2);
        }
};
