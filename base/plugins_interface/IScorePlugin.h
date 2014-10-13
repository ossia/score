#pragma once

#include <vector>
#include <utility>
#include <memory>
#include <iostream>
#include <plugins_interface/capabilities/process/ProcessCapabilityFactory.h>

// NOTE : penser à gérer les cas ou des plug-ins sont absents ? e.g. un time process pas là (cf. live qui désactive)
// class DragCommand {};
// class DropOnProcessCommand {};
// class DropAction{};




class ExampleTimeProcessFactory : public ProcessCapabilityFactory
{
public:
	ExampleTimeProcessFactory():
		ProcessCapabilityFactory("DatTimeProcess")
	{
	}
};
// Pour faire un TimeProcess, on hérite de ProcessImplementation.
class IScorePlugin
{
	
public:
	std::vector<AbstractCapabilityFactoryInterface_p> m_pluginCapabilities;
};