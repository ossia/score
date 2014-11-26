#pragma once
#include <QNamedObject>
#include <interface/process/ProcessViewModelInterface.hpp>

class ScenarioProcessViewModel : public iscore::ProcessViewModelInterface
{
	Q_OBJECT
	
	public:
		ScenarioProcessViewModel(int id, QObject* parent);
		virtual ~ScenarioProcessViewModel() = default;
		
	private:
	
};

