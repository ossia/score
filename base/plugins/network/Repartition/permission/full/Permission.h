#pragma once
#include "../PermissionBase.h"

class Permission : public PermissionBase
{
	public:
		enum class Category   : int  { Read = 0, Write = 1, Execute = 2 };
		enum class Enablement : bool { Enabled = true, Disabled = false };

		using PermissionBase::PermissionBase;
		virtual ~Permission() = default;

		virtual bool listens() const override
		{
			return read or write or exec;
		}

		virtual bool writes() const override
		{
			return write;
		}

		void setPermission(Permission::Category cat,
						   Permission::Enablement enablement)
		{
			switch(cat)
			{
				case Category::Read:
					read = static_cast<bool>(enablement);
					break;
				case Category::Write:
					write = static_cast<bool>(enablement);
					break;
				case Category::Execute:
					exec = static_cast<bool>(enablement);
					break;
				default:
					throw;
			}
		}

		bool getPermission(Permission::Category cat)
		{
			switch(cat)
			{
				case Category::Read:
					return read;
				case Category::Write:
					return write;
				case Category::Execute:
					return exec;
				default:
					throw;
			}
		}

	private:
		bool read = false;
		bool write = false;
		bool exec = false;
};
