#pragma once

#include <interface/panel/PanelModelInterface.hpp>
namespace iscore
{
	class Model;
}
/**
 * @brief The InspectorPanelModel class
 *
 * @todo Inspector Model : keep the currently identified object. (ObjectPath)
 */
class InspectorPanelModel : public iscore::PanelModelInterface
{
		Q_OBJECT
	public:
		InspectorPanelModel(iscore::Model* parent);
		
	signals:
		void setNewItem(QObject*);
		
	public slots:
		void newItemInspected(QObject*); 
		
	private:
		QObject* m_selectedObject{}; // TODO get Path instead.
};
