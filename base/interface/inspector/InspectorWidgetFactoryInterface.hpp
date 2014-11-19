#pragma once
#include <QObject>
class QWidget;

namespace iscore
{
	/**
	 * @brief The InspectorWidgetFactoryInterface class
	 * 
	 * This class has to be registered in the inspector for each plug-in that 
	 * provides it.
	 * 
	 * When an object in the Document is selected, the pointer to the object is sent 
	 * to the inspector using the signal-slot mechanism.
	 * The factory can then make a widget from the QObject, which can be displayed 
	 * in the inspector.
	 * 
	 */
	class InspectorWidgetFactoryInterface : public QObject
	{
		public:
			/**
			 * @brief makeWidget Makes a widget for the inspector from an object
			 * @param sourceElement Element from which an inspector widget is to be made
			 * @return An inspector widget corresponding to the object.
			 */
			QWidget* makeWidget(QObject* sourceElement);
			
			/**
			 * @brief makeWidget Makes a widget for the inspector from a list of objects
			 * @param sourceElements List of elements
			 * @return An inspector widget corresponding to the objects
			 */
			QWidget* makeWidget(QList<QObject*> sourceElements);
	};
}