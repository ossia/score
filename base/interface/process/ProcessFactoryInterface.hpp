#pragma once
#include <QStringList>
#include <QObject>

namespace iscore
{
	class ProcessModelInterface;
	class ProcessViewInterface;
	class ProcessPresenterInterface;

	/**
	 * @brief The ProcessFactoryInterface class
	 *
	 * Interface to make processes, like Scenario, Automation...
	 */
	class ProcessFactoryInterface
	{
		public:
			virtual ~ProcessFactoryInterface() = default;

			// The process name
			virtual QString name() const = 0;

			virtual QStringList availableViews() = 0;
			virtual ProcessViewInterface* makeView(QString view) = 0;
			virtual ProcessPresenterInterface* makePresenter() = 0;

			// Behind the scene, an API object.
			// Also contains all the drag&drop stuff? Or is more specifically in TimeProcess?
			virtual ProcessModelInterface* makeModel(unsigned int id, QObject* parent)  = 0; // Accédé par les commandes uniquement.
	};
}
