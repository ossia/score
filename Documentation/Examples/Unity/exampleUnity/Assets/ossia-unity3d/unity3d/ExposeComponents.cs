using System;
using System.Reflection;
using System.Collections.Generic;
using UnityEngine;

namespace Ossia
{
  //! Expose an object through ossia.
  //! This shows all the components put in the component list
  public class ExposeComponents : ExposedObject
  {
    public string NodeName = "Object";
    public List<Component> components;

    Ossia.Node scene_node;

    void RegisterComponent (Component component, Ossia.Node node)
    {
      var flags = BindingFlags.Public | BindingFlags.Instance | BindingFlags.DeclaredOnly;
      var fields = component.GetType ().GetFields (flags);
      var properties = component.GetType ().GetProperties ();

      if (fields.Length == 0 && properties.Length == 0)
        return;
      
      // Create a node for the component
      var ossia_c = new OssiaEnabledComponent (component, node);

      ossia_components.Add (ossia_c);

      // Create nodes for all the fields that were exposed
      foreach (var field in fields) {
        // Get the type of the field
        ossia_type t;
        try {
          t = Ossia.Value.GetOssiaType (field.FieldType);
        } catch {
          continue;
        }

        // Create an attribute to bind the field.
        var oep = new OssiaEnabledField (field, field.Name, ossia_c, t);
        ossia_c.fields.Add (oep);
        controller.Register (oep);
      }


      // Create nodes for all the fields that were exposed
      foreach (PropertyInfo prop in properties) {
        var prop_t = prop.PropertyType;
        Debug.Log ("Prop: " + prop.Name + " => " + prop.PropertyType.ToString());
        if (prop_t == typeof(UnityEngine.Transform)) { 
          // TODO
        } else if (prop_t == typeof(UnityEngine.Component)) { 
          try { 
            var subcomp = (Component)prop.GetValue (component, null);
            var subnode = node.AddChild (prop.Name);
            RegisterComponent (subcomp, subnode);
          } catch {
          }
        } else { 
          // Get the type of the field
          ossia_type t;
          try {
            t = Ossia.Value.GetOssiaType (prop_t);
          } catch { 
            continue;
          }

          var oep = new OssiaEnabledProperty (prop, prop.Name, ossia_c, t);
          ossia_c.properties.Add (oep);
          controller.Register (oep);
        }
      }
    }

    public override void Start ()
    {
      if (child_node != null) {
        Debug.Log ("Object already registered.");        
        return;
      }

      controller = Controller.Get ();
      scene_node = controller.SceneNode ();

      child_node = scene_node.AddChild (NodeName);
      foreach (var c in components) {
        var subnode = child_node.AddChild (c.GetType ().Name);
        RegisterComponent (c, subnode);
      }
    }
  }
}
