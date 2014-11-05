#pragma once
#include <vector>
#include "PermissionView.h"

#include "../../Iterable.h"
class PermissionViewManager : public Iterable<PermissionView>
{
	public:
		void startsListening(Client& client, Group& group)
		{
			std::cerr << "I take notice that client " << client.getName() << " listens " << group.getName() << std::endl;
			(*this)(group, client)._listens = true;
		}

		void stopsListening(Client& client, Group& group)
		{
			(*this)(group, client)._listens = false;
		}

		void startsWriting(Client& client, Group& group)
		{
			(*this)(group, client)._writes = true;
		}

		void stopsWriting(Client& client, Group& group)
		{
			(*this)(group, client)._writes = false;
		}
};

