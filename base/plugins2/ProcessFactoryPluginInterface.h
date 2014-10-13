#pragma once
#include <QStringList>
class TimeProcess { };
class IScoreProcessView { };

class IScoreProcessSmallView { };
class IScoreProcessStandardView { };
class IScoreProcessFullView { };

// Pour signaux / slots auto-connectants : 
// Comme on ne peut pas mettre d'héritage depuis QObject, 
// il faut encapsuler le QObject qui contiendra les signaux & slots.
// Pour faire de l'introspection sur les signaux/slots dispos au runtime, 
// on peut utiliser QMetaObject qui permet de faire des checks.

// Premier example simple à faire : panel qui se met à jour à la 
// sélection d'un process.
class IScoreProcessPresenter 
{ 
	// Gérer ici actions sur GUI, drag&drop, et modifications du modèle.
};

class IScoreProcessModel 
{
private:
	TimeProcess process;
};

class IScoreProcess
{
	// Behind the scene, an API object.
	// Also contains all the drag&drop stuff? Or is more specifically in TimeProcess?
	
	QStringList availableViews() = 0;
	std::unique_ptr<IScoreProcessView> makeView(QString view) = 0;
	// Mission : transmettre au présenteur global pour validation de l'action.
	// Ou bien c'est directement la vue qui s'en charge? 
	// Risque de duplication dans le cas SmallView / StandardView / FullView...
	std::unique_ptr<IScoreProcessPresenter> makePresenter() = 0; 
	std::unique_ptr<IScoreProcessModel> makeModel() = 0; // Accédé par les commandes uniquement. 	
};
class IScoreProcessFactoryPluginInterface
{
	public:
		// List the Processes offered by the plugin.
		// Note : here we don't use camel case for the "process_" part because it is more of a namespacing required 
		// by the fact that if a plugin implements for instance a process and a panel, 
		// Qt requires the creation of the class which inherits both the process interface and the panel interface
		// which might cause name collisions.
		QStringList process_list() = 0;
		std::unique_ptr<IScoreProcess> process_make(QString name) = 0;
};