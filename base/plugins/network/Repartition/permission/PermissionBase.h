#pragma once
#include "../group/Group.h"
#include "../client/Client.h"

class PermissionBase
{
	public:
		static const std::function<bool(PermissionBase&)> hasSame(const Group& g,
																  const Client& c)
		{
			return [&g, &c] (const PermissionBase& p)
			{ return *p._group == g && *p._client == c; };
		}

		static const std::function<bool(PermissionBase&)> hasSame(const Client& c,
																  const Group& g)
		{
			return [&g, &c] (const PermissionBase& p)
			{ return *p._group == g && *p._client == c; };
		}

		static const std::function<bool(const PermissionBase&)> hasSame(const Group& g)
		{
			return [&g] (const PermissionBase& p)
			{ return *p._group == g; };
		}

		static const std::function<bool(const PermissionBase&)> hasSame(const Client& c)
		{
			return [&c] (const PermissionBase& p)
			{ return *p._client == c; };
		}

		PermissionBase(const Group& group,
					   const Client& client):
			_group(&group),
			_client(&client)
		{
		}

		PermissionBase(PermissionBase&&) = default;
		PermissionBase(const PermissionBase&) = default;
		PermissionBase& operator=(PermissionBase&&) = default;
		PermissionBase& operator=(const PermissionBase&) = default;
		virtual ~PermissionBase() = default;

		bool operator==(const PermissionBase& p)
		{
			return *p._group == *_group &&
				   *p._client == *_client;
		}

		virtual bool listens() const = 0;
		virtual bool writes()  const = 0;

		const Client& getClient()
		{
			return *_client;
		}

	private:
		const Group*  _group;
		const Client* _client;
};
