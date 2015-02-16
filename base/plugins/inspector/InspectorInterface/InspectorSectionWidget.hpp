#pragma once

#include <QWidget>
class QVBoxLayout;
class QLineEdit;
class QToolButton;
class QPushButton;
class QScrollArea;

namespace iscore
{
	class SerializableCommand;
}
/** @brief InspectorSectionWidget is widget that can fold or unfold his content.
 *
 * A header with a name is always displayed.
 * It contains one main widget in a QScrollArea with a vertical layout, that can be folded/unfolded on click on the arrow button.
 */

class InspectorSectionWidget : public QWidget
{
		Q_OBJECT
	public:
		explicit InspectorSectionWidget (QWidget* parent = 0);
		InspectorSectionWidget (QString name, QWidget* parent = 0);
		~InspectorSectionWidget();

	signals:
		void submitCommand(iscore::SerializableCommand*);

	public slots:

		// Display tool
		void expand();

		// Manage section

		// Removes all the content.
		void clear();

		//! change the name in the header
		void renameSection (QString newName);

		//! add the widget newWidget in the main layout
		void addContent (QWidget* newWidget);

		//! removes the widget from the main layout
		void removeContent(QWidget* toRemove);

        void removeAll();

		//! insert newWidget at the index rank in the main layout
		void insertInSection (int index, QWidget* newWidget);

		void nameEditEnable();
		void nameEditDisable();

	private:
		QWidget* _container = nullptr;
		QVBoxLayout* _containerLayout = nullptr; /*!< main layout */

		QLineEdit* _sectionTitle = nullptr; /*!< header label \todo editable ? */
		QToolButton* _btn = nullptr; /*!< button for the fold/unfold action */

		QPushButton* _buttonTitle;

		bool _isUnfolded;
};
