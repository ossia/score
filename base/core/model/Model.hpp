#pragma once
#include <QObject>
#include <set>

namespace iscore
{
	class PanelModelInterface;
	/**
	 * @brief The Model class holds the models of the panels.
	 * 
	 * @todo{should not the DocumentModel be here ?}
	 */
	class Model : public QObject
	{
		public:
			using QObject::QObject;
			void addPanel(PanelModelInterface*);

		private:
			std::set<PanelModelInterface*> m_panelsModels;
	};
}
