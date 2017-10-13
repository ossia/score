using System.Collections.Generic;
using System.Runtime;
using System.Runtime.InteropServices;
using System;
using UnityEngine;
using UnityEngine.Assertions;

public class CustomDevice : MonoBehaviour
{

  // Use this for initialization
  Ossia.Device local;
  float val = 0;

  void OnApplicationQuit()
  {
    Ossia.Network.ossia_device_reset_static ();
  }

  public delegate void debug_log_delegate(string str);
  void Start ()
  {
    Application.runInBackground = true;

    var callback_delegate = new debug_log_delegate ((string str) => Debug.Log("OSSIA : " + str));
    IntPtr intptr_delegate =  Marshal.GetFunctionPointerForDelegate (callback_delegate);
    Ossia.Network.ossia_set_debug_logger (intptr_delegate);

    local = new Ossia.Device (new Ossia.OSCQuery (1234, 5678), "newDevice");

    var root = local.GetRootNode ();
    Assert.AreEqual (root.ChildSize (), 0);

    // Just for tests:
    {
      // Create a node and an address
      var bar = Ossia.Node.CreateNode (root, "/foo/bar");
      var addr = bar.CreateParameter (Ossia.ossia_type.VEC3F);
      Assert.AreEqual (root.ChildSize (), 1);

      var foo = root.GetChild (0);
      Assert.AreEqual (foo.GetName (), "foo");

      // Try removing the address
      Assert.AreNotEqual (bar.GetNode (), IntPtr.Zero);
      Assert.AreEqual (bar.GetParameter (), addr);

      foo.RemoveChild (foo.GetChild (0));
      Assert.AreEqual (bar.GetNode (), IntPtr.Zero);
      Assert.AreEqual (bar.GetParameter (), null);
    }

    {
      var str = Ossia.Node.CreateNode (root, "/my_string");
      var addr = str.CreateParameter (Ossia.ossia_type.STRING);
      addr.PushValue (new Ossia.Value ("some string !"));
      Debug.Log(addr.GetValue ().GetString ());
    }

    {
      var blu = Ossia.Node.CreateNode (root, "/foo/blu");
      blu.CreateParameter (Ossia.ossia_type.VEC3F);
    }

    {
      Ossia.Node.CreatePattern (root, "/{boo,bzu}/zaza.[0-5]");
      Ossia.Node.FindPattern (root, "/{boo,bzu}/zaza.[0-5]");
    }

    {
      var array = Ossia.Node.CreateNode (root, "/my_array");
      Ossia.Parameter addr2 = array.CreateParameter (Ossia.ossia_type.LIST);
      addr2.PushValue (new int[]{ 1, 2, 4, 65 });
      for(int i = 0; i < 4; i++)
        Debug.Log(addr2.GetValue().GetIntArray()[i]);
    }
  }

  // Update is called once per frame
  void Update ()
  {
    if (local != null) {
      val += 0.1f;
      var node = Ossia.Node.FindNode (local.GetRootNode (), "/foo/blu");
      node.GetParameter ().PushValue (val, val, val);
    }
  }
}
