#pragma once
#include <interface/documentdelegate/DocumentDelegateModelInterface.hpp>

class ScenarioCentralPanelModel  : public iscore::DocumentDelegateModelInterface
{
	public:
		ScenarioCentralPanelModel() :
			iscore::DocumentDelegateModelInterface {nullptr}
		{

		}
		virtual ~ScenarioCentralPanelModel() = default;
};
