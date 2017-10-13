using System;
using System.Reflection;
using System.Collections.Generic;
using UnityEngine;

namespace Ossia
{
  public abstract class ExposedObject : MonoBehaviour
  {
    public bool sendUpdates = false;
    public Ossia.Node child_node;
    internal List<OssiaEnabledComponent> ossia_components = new List<OssiaEnabledComponent>();
    protected Ossia.Controller controller = null;

    public abstract void Start();
    public void ReceiveUpdates()
    {
      if (child_node == null) {
        Start ();
      }

      if (!child_node.GetValueUpdating ()) 
        child_node.SetValueUpdating (true);

      foreach (var component in ossia_components) {
        component.ReceiveUpdates ();
      }
    }

    public void SendUpdates()
    {
      if (child_node == null) {
        Start ();
      }

      if(child_node.GetValueUpdating())
        child_node.SetValueUpdating (false);

      foreach (var component in ossia_components) {
        component.SendUpdates ();
      }
    }

    public void Update()
    {
      if (child_node == null) {
        Debug.Log("Object not registered.");  
        Start ();
      }

      if (sendUpdates) {
        SendUpdates ();
      }
    }

    ~ExposedObject()
    {
      foreach (var c in ossia_components) {
        foreach (var field in c.fields) {
          //controller.Unregister (field);
        }
        foreach (var prop in c.properties) {
          //controller.Unregister (prop);
        }
      }
    }

  }
}