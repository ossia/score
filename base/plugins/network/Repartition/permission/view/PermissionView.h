#pragma once
#include "../PermissionBase.h"

/**
 * @brief The PermissionView class
 *
 * Holds the data required for another client to take proper actoin
 * regarding to a client in the same group.
 */
class PermissionViewManager;
class PermissionView : public PermissionBase
{
		friend class PermissionViewManager;
	public:
		using PermissionBase::PermissionBase;
		virtual ~PermissionView() = default;

		virtual bool listens() const
		{
			return _listens;
		}

		virtual bool writes()  const
		{
			return _writes;
		}

	private:
		bool _listens;
		bool _writes;
};
