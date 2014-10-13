#pragma once

// A BaseElement : the basic view of i-score. (what appears when you make "New").
// For now it will mostly be BaseScenario I guess.
class IScoreBaseElement
{
};

class IScoreBaseElementFactoryPluginInterface
{
	public:
		QStringList baseElement_list() = 0;
		std::unique_ptr<IScoreBaseElement> baseElement_make(QString name) = 0;
};