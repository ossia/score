using System;
using System.Reflection;
using System.Collections.Generic;
using UnityEngine;

namespace Ossia
{

  //! Expose an object through ossia.
  //! This shows only the transform and the fields explicitely marked with [Ossia.Expose].
  public class ExposeAttributes : ExposedObject
  {
    void RegisterComponent(Component component)
    {
      const BindingFlags flags = BindingFlags.Public | BindingFlags.Instance | BindingFlags.DeclaredOnly;
      FieldInfo[] fields = component.GetType().GetFields(flags);

      // Find the fields that are marked for exposition
      OssiaEnabledComponent ossia_c = null;
      foreach (FieldInfo field in fields) {
        if(Attribute.IsDefined(field, typeof(Ossia.Expose))) 
        {
          // Get the type of the field
          ossia_type t;
          try { t = Ossia.Value.GetOssiaType (field.FieldType); } catch { continue; }

          // Create a node for the component
          if (ossia_c == null) {
            ossia_c = makeComponent(component);
          }

          // Create an attribute to bind the field.
          var attr = (Ossia.Expose) Attribute.GetCustomAttribute(field, typeof(Ossia.Expose));

          var oep = new OssiaEnabledField (field, attr.ExposedName, ossia_c, t);
          ossia_c.fields.Add(oep);
          controller.Register (oep);
        }
      }

      foreach (PropertyInfo prop in component.GetType().GetProperties ()) 
      {
        if(Attribute.IsDefined(prop, typeof(Ossia.Expose))) {

          // Get the type of the property;
          ossia_type t;
          try { t = Ossia.Value.GetOssiaType (prop.PropertyType); } catch { continue; }

          // Create a node for the component
          if (ossia_c == null) {
            ossia_c = makeComponent(component);
          }

          var attr = (Ossia.Expose) Attribute.GetCustomAttribute(prop, typeof(Ossia.Expose));

          // Create an attribute to bind the property.
          var oep = new OssiaEnabledProperty(prop, attr.ExposedName, ossia_c, t);
          ossia_c.properties.Add(oep);
          controller.Register (oep);
        }
      }
    }

    void RegisterTransform()
    {
      var component = this.transform;
      var ossia_c = new OssiaEnabledComponent(component, child_node.AddChild("transform"));
      ossia_components.Add (ossia_c);

      foreach (PropertyInfo prop in component.GetType().GetProperties ()) 
      {
        if(validTransformType (prop.PropertyType)) 
        {
          // Get the type of the property;
          ossia_type t;
          try { t = Ossia.Value.GetOssiaType (prop.PropertyType); } catch { continue; }

          // Create an attribute to bind the property.
          var oep = new OssiaEnabledProperty(prop, prop.Name, ossia_c, t);
          ossia_c.properties.Add (oep);
          controller.Register (oep);
        }
      }
    }


    public override void Start()
    {
      if (child_node != null) {
        Debug.Log("Object already registered.");        
        return;
      }

      controller = Controller.Get ();
      scene_node = controller.SceneNode ();
      if (scene_node == null) {
        Debug.Log("scene_node == null");  
        return;
      }

      child_node = scene_node.AddChild(this.gameObject.name);

      // For each component, we check the public fields.
      // If these fields have the Ossia.Expose attribute,
      // then we create node structures for them.
      foreach (var component in this.gameObject.GetComponents <Component>()) {
        RegisterComponent (component);
      }

      // We also register the transform in all cases.
      RegisterTransform ();
    }


    bool validTransformType(Type t) { 
      return t == typeof(Vector2) 
        || t == typeof(Vector3) 
        || t == typeof(Vector4) 
        || t == typeof(Quaternion);
    }

    OssiaEnabledComponent makeComponent(Component c)
    {
      var comp = new OssiaEnabledComponent(c, child_node.AddChild(c.GetType().ToString()));
      ossia_components.Add (comp);
      return comp;        
    }

    Ossia.Node scene_node;
  }
}

