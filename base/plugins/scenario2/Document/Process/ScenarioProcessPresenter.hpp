#pragma once
#include <interface/process/ProcessPresenterInterface.hpp>
namespace iscore
{
	class ProcessViewModelInterface;
}
class ScenarioProcessPresenter : public iscore::ProcessPresenterInterface
{
	Q_OBJECT

	public:
		ScenarioProcessPresenter(iscore::ProcessViewModelInterface* model, QObject* parent);
		virtual ~ScenarioProcessPresenter() = default;

	private:

};

