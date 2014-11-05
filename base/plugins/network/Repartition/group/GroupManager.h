#pragma once

#include "Group.h"
#include "../Iterable.h"
#include <utility>
#include <algorithm>
class GroupManager : public Iterable<Group>
{
	private:
		int _lastId = 0;

	public:
		Group& createGroup(std::string name)
		{
			performUniquenessCheck(name);
			auto& g = create(name, _lastId++);

			return g;
		}

		Group& createGroup(int id, std::string name)
		{
			performUniquenessCheck(id);
			performUniquenessCheck(name);
			auto& g = create(name, id);

			_lastId = std::max(_lastId, id);

			return g;
		}
};
