#pragma once
#include <memory>
#include "../Scenario.h"
#include "group/Group.h"
#include "synchronisation_policy/SynchronisationPolicy.h"

using namespace OSSIA;
class DistributedScenario : public Scenario
{
	public:
		// Reimplementation of the Scenario interface
		// Lecture
		virtual void play() const override
		{
			_scenario->play();
		}

		// Navigation
		std::set<TimeBox*> getTimeBoxes() const
		{
			return _scenario->getTimeBoxes();
		}

		std::set<TimeNode*> getTimeNodes() const
		{
			return _scenario->getTimeNodes();
		}

		// DistributedScenario starts here
		DistributedScenario(Scenario* sc):
			_scenario(sc)
		{
		}

		virtual ~DistributedScenario()
		{
			removeFromGroup();
		}

		void setSynchronisationPolicy(SynchronisationPolicy_p&& syncpol)
		{
			_sync = std::move(syncpol);
		}

		void removeFromGroup()
		{
			if(_group) _group->removeScenario(*this);
			_group = nullptr;
			
		}

		void assignToGroup(Group& g)
		{
			if(!_group || g != *_group)
			{
				removeFromGroup();
				g.addScenario(*this);
				_group = &g;
			}
		}

		void assignToGroupRecursively(Group& g)
		{
			assignToGroup(g);
			if(!isLocked)
			{
				for(TimeBox* b : getTimeBoxes())
				{
					// for timeProcess : b.begin()
					//   if(DistributedScenario* s = dynamic_cast<DistributedScenario>(timeProcess))
					//		s.assignToGroupRecursively(g);
				}
			}
		}

		void lockToGroup()
		{
			isLocked = true;
		}

		void unlockToGroup()
		{
			isLocked = false;
		}

	private:
		SynchronisationPolicy_p _sync;
		Group* _group = nullptr;
		bool isLocked = false;

		std::unique_ptr<Scenario> _scenario{nullptr};
};

