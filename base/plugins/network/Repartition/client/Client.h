#pragma once
#include <string>
#include <memory>
#include "../osc/oscreceiver.h"

#include "../group/Group.h"

#include"../properties/hasId.h"
#include"../properties/hasName.h"
class Client : public hasId, public hasName
{
	public:
		using hasName::hasSame;
		using hasId::hasSame;

		Client(const std::string& hostname):
			Client(-1, hostname)
		{
		}

		Client(const int id,
			   const std::string& hostname):
			hasId(id),
			hasName(hostname)
		{
		}

		virtual ~Client() = default;
		Client(Client&&) = default;
		Client(const Client&) = default;
		Client& operator=(const Client&) = default;
		Client& operator=(Client&&) = default;

		bool operator==(const Client& c) const
		{
			return c.getId() == getId();
		}

		bool operator!=(const Client& c) const
		{
			return c.getId() != getId();
		}
};

using Client_p = std::unique_ptr<Client>;
