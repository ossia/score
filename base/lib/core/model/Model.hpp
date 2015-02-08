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

	// TODO remove. It is not useful. The models should be completely in the documents;
	// the software model is materialized by the Settings.
	class Model : public QObject
	{
		public:
			using QObject::QObject;

		private:
			std::set<PanelModelInterface*> m_panelsModels;
	};
}
