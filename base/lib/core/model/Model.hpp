#pragma once
#include <QObject>
#include <set>

namespace iscore
{
	class PanelModelInterface;
	/**
	 * @brief The Model class holds the models of the panels.
	 *
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
