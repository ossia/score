#include <iostream>
#include "IScorePlugin.h"

int main(int argc, char **argv) 
{
	// 1.
	ProcessCapabilityFactory s{"roulade"};
	s.make(3);
	
	std::cerr << s.capabilityName() << " " << s.processName() << std::endl;
	
	// 2.
	IScorePlugin p;
	p.m_pluginCapabilities.push_back(std::make_unique<ExampleTimeProcessFactory>());
     
	for(auto& fac : p.m_pluginCapabilities)
	{
		if(fac->capabilityName() == "Process") // Find a better way to do this ? 
		{
			auto proc_fac = dynamic_cast<ProcessCapabilityFactory*>(fac.get());
			std::cerr << "The process is called : " << proc_fac->processName() << std::endl;
		}
	}
    return 0;
}
