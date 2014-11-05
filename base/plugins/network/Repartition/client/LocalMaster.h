#pragma once
#include "LocalClient.h"

class LocalMaster: public LocalClient
{
	public:
		using LocalClient::LocalClient;

		virtual ~LocalMaster() = default;
};
