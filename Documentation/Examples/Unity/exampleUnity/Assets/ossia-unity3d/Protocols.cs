using System;

namespace Ossia
{
	public class Protocol
	{
		internal IntPtr ossia_protocol;
		protected Protocol(IntPtr impl)
		{
			ossia_protocol = impl;
		}
	}

	public class Local : Protocol
	{
		public Local() :
		base(Network.ossia_protocol_multiplex_create())
		{
		}

		public void ExposeTo(Protocol other)
		{
			Network.ossia_protocol_multiplex_expose_to (ossia_protocol, other.ossia_protocol);
		}
	}

	public class Minuit : Protocol
	{
		public Minuit(string local_name, string ip, int in_port, int out_port) :
		base(Network.ossia_protocol_minuit_create(local_name, ip, in_port, out_port))
		{
		}
	}

	public class OSCQuery : Protocol
	{
		public OSCQuery(int osc_port, int ws_port) :
		base(Network.ossia_protocol_oscquery_server_create(osc_port, ws_port))
		{
		}
	}

	public class OSC : Protocol
	{
		public OSC(string ip, int in_port, int out_port) :
		base(Network.ossia_protocol_osc_create(ip, in_port, out_port))
		{
		}
	}
}
