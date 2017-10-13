using System;
using System.Reflection;
using System.Collections.Generic;
using UnityEngine;

namespace Ossia
{
  internal interface OssiaEnabledElement
  {
    void ReceiveUpdates ();
  }

  //! Used to register C# fields to Ossia
  internal class OssiaEnabledField : OssiaEnabledElement
  {
    public OssiaEnabledComponent parent;

    public FieldInfo field;
    public string attribute;

    public Ossia.Node ossia_node;
    public Ossia.Parameter ossia_parameter;

    object previousValue;

    public OssiaEnabledField (FieldInfo f, string attr, OssiaEnabledComponent par, ossia_type t)
    {
      field = f;
      attribute = attr;
      parent = par;

      ossia_node = parent.component_node.AddChild (attr);
      ossia_parameter = ossia_node.CreateParameter (f.FieldType, t);
      SendUpdates ();
    }

    //! Ossia -> C#
    public void ReceiveUpdates ()
    {
      try {
        var value = ossia_parameter.GetValue ();
        var cur_val = field.GetValue (parent.component);
        var new_val = value.Get (field.FieldType);

        if (!new_val.Equals (cur_val)) {
          field.SetValue (parent.component, new_val);
        }
      } catch (Exception e) {
        Debug.LogWarning (e.Message);
      }
    }

    //! C# -> Ossia
    public void SendUpdates ()
    {
      var val = field.GetValue (parent.component);
      if (!val.Equals (previousValue)) {
        previousValue = val;
        ossia_parameter.PushValue (new Value (val));
      }
    }
  }

  //! Used to register C# properties to Ossia
  internal class OssiaEnabledProperty : OssiaEnabledElement
  {
    public OssiaEnabledComponent parent;

    public PropertyInfo field;
    public string attribute;

    public Ossia.Node ossia_node;
    public Ossia.Parameter ossia_parameter;

    object previousValue;

    public OssiaEnabledProperty (PropertyInfo f, string attr, OssiaEnabledComponent par, ossia_type t)
    {
      field = f;
      attribute = attr;
      parent = par;

      ossia_node = parent.component_node.AddChild (attr);
      ossia_parameter = ossia_node.CreateParameter (f.PropertyType, t);
      SendUpdates ();
    }

    //! Ossia -> C#
    public void ReceiveUpdates ()
    {
      if (field.CanRead && field.CanWrite) {   
        try {
          var value = ossia_parameter.GetValue ();
          var cur_val = field.GetValue (parent.component, null);
          var new_val = value.Get (field.PropertyType);

          if (!new_val.Equals (cur_val)) {
            field.SetValue (parent.component, new_val, null);
          }
        } catch (Exception e) {
          Debug.LogWarning (e.Message);
        }
      }
    }

    //! C# -> Ossia
    public void SendUpdates ()
    {
      if (field.CanRead) {
        var val = field.GetValue (parent.component, null);
        if (!val.Equals (previousValue)) {
          previousValue = val;
          ossia_parameter.PushValue (new Value (val));
        }
      }
    }
  }

  //! A component whose fields have some [Ossia.Expose] attributes
  internal class OssiaEnabledComponent
  {
    public Component component;
    public Ossia.Node component_node;

    public List<OssiaEnabledField> fields = new List<OssiaEnabledField> ();
    public List<OssiaEnabledProperty> properties = new List<OssiaEnabledProperty> ();

    public OssiaEnabledComponent (Component comp, Ossia.Node node)
    {
      component = comp;
      component_node = node;
    }

    //! Ossia -> C#
    public void ReceiveUpdates ()
    {
      foreach (var field in fields) {
        field.ReceiveUpdates ();
      }
      foreach (var property in properties) {
        property.ReceiveUpdates ();
      }
    }

    //! C# -> Ossia
    public void SendUpdates ()
    {
      foreach (var field in fields) {
        field.SendUpdates ();
      }
      foreach (var property in properties) {
        property.SendUpdates ();
      }
    }
  }
}