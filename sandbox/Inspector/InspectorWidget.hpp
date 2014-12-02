#pragma once
#include <QObject>
#include "InspectorWidgetFactoryInterface.hpp"

class InspectorWidgetInterface;

//namespace iscore
//{
	/**
     * @brief The InspectorWidget class
	 * 
     * This class implement the InspectorWidgetFactoryInterface.
	 * 
	 * When an object in the Document is selected, the pointer to the object is sent 
	 * to the inspector using the signal-slot mechanism.
	 * The factory can then make a widget from the QObject, which can be displayed 
	 * in the inspector.
	 * 
	 */
    class InspectorWidget : public InspectorWidgetFactoryInterface
	{
        public:
            InspectorWidget():
                InspectorWidgetFactoryInterface{}
            {

            }
			/**
			 * @brief makeWidget Makes a widget for the inspector from an object
			 * @param sourceElement Element from which an inspector widget is to be made
			 * @return An inspector widget corresponding to the object.
			 */
            InspectorWidgetInterface* makeWidget(QObject* sourceElement) override;
			
			/**
			 * @brief makeWidget Makes a widget for the inspector from a list of objects
			 * @param sourceElements List of elements
			 * @return An inspector widget corresponding to the objects
			 */
            InspectorWidgetInterface* makeWidget(QList<QObject*> sourceElements) override;
	};
//}
