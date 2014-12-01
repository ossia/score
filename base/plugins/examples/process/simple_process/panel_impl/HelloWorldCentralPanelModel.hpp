#pragma once
#include <interface/documentdelegate/DocumentDelegateModelInterface.hpp>

class HelloWorldCentralPanelModel  : public iscore::DocumentDelegateModelInterface
{
	public:
		HelloWorldCentralPanelModel():
			iscore::DocumentDelegateModelInterface{nullptr}
		{

		}
		virtual ~HelloWorldCentralPanelModel() = default;

};
