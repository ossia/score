#pragma once

#include <QString>
#include <QWidget>
#include <iscore_lib_inspector_export.h>
class QLineEdit;
class QPushButton;
class QToolButton;
class QVBoxLayout;

namespace Inspector
{
/** @brief InspectorSectionWidget is widget that can fold or unfold his content.
 *
 * A header with a name is always displayed.
 * It contains one main widget in a QScrollArea with a vertical layout, that can be folded/unfolded on click on the arrow button.
 */

class ISCORE_LIB_INSPECTOR_EXPORT InspectorSectionWidget : public QWidget
{
        Q_OBJECT
    public:
        explicit InspectorSectionWidget(bool editable = false, QWidget* parent = 0);
        InspectorSectionWidget(QString name, bool nameEditable = false, QWidget* parent = 0);
        virtual ~InspectorSectionWidget();

    public slots:

        // Display tool
        void expand();

        // Manage section

        //! change the name in the header
        void renameSection(QString newName);

        //! add the widget newWidget in the main layout
        void addContent(QWidget* newWidget);

        //! removes the widget from the main layout
        void removeContent(QWidget* toRemove);

        void removeAll();

    signals:
        void nameChanged(QString newName);

    protected:
        virtual QWidget* titleWidget();
        QLineEdit* m_sectionTitle = nullptr; /*!< header label editable ? */

    private:
        QWidget* m_container = nullptr;
        QVBoxLayout* m_containerLayout = nullptr; /*!< main layout */

        QToolButton* m_btn = nullptr; /*!< button for the fold/unfold action */

        QPushButton* m_buttonTitle;

        bool m_isUnfolded;
};
}
