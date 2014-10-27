#pragma once
#include <string>
class RemoteActionReceiver
{
	public:
		virtual void onReceive(std::string, std::string, const char*, int) = 0;
};
