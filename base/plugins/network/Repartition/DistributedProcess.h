#pragma once
#include <TimeProcess.h>

#include <group/Group.h>
#include <synchronisation_policy/SynchronisationPolicy.h>
using namespace OSSIA;
class DistributedProcess : public TimeProcess
{
	public:
		virtual void play() const
		{
		}

		
		// DistributedScenario starts here
		DistributedProcess(TimeProcess* sc):
			_scenario(sc)
		{
		}

		virtual ~DistributedProcess()
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
/*
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
*/
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

		std::unique_ptr<TimeProcess> _scenario{nullptr};

};


