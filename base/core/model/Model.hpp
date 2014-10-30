#pragma once
#include <QObject>
#include <set>

namespace iscore
{
	class PanelModel;
	/**
	 * @brief The Model class holds the models of the panels.
	 * 
	 * @todo{should not the DocumentModel be here ?}
	 */
	class Model : public QObject
	{
		public:
			using QObject::QObject;
			void addPanel(PanelModel*);

		private:
			std::set<PanelModel*> m_panelsModels;
	};
}
