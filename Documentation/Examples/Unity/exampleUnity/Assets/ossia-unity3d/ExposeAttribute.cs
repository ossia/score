using System.Runtime;
using System.Runtime.InteropServices;
using System.Collections.Generic;
using System;

namespace Ossia
{
	[System.AttributeUsage(System.AttributeTargets.All)]
	public class Expose : System.Attribute
	{
		public string ExposedName;

		public Expose()
		{
			this.ExposedName = String.Empty;
		}
		public Expose(string name)
		{
			this.ExposedName = name;
		}
	}
}
