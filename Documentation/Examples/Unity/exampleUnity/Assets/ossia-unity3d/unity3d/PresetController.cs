using UnityEngine;
using System.Collections;
using Namespace;
using Ossia;
using System;
using System.IO;
using System.Text;
using System.Collections.Generic;
using System.Runtime.InteropServices;

#if UNITY_EDITOR
using UnityEditor;
#endif
using System.Reflection;

[System.AttributeUsage (System.AttributeTargets.Field)]
public class InspectorButtonAttribute : PropertyAttribute
{
  public static float kDefaultButtonWidth = 80;

  public readonly string MethodName;

  private float _buttonWidth = kDefaultButtonWidth;

  public float ButtonWidth {
    get { return _buttonWidth; }
    set { _buttonWidth = value; }
  }

  public InspectorButtonAttribute (string MethodName)
  {
    this.MethodName = MethodName;
  }
}

[CustomPropertyDrawer (typeof(InspectorButtonAttribute))]
public class InspectorButtonPropertyDrawer : PropertyDrawer
{
  private MethodInfo _eventMethodInfo = null;

  public override void OnGUI (Rect position, SerializedProperty prop, GUIContent label)
  {
    InspectorButtonAttribute inspectorButtonAttribute = (InspectorButtonAttribute)attribute;
    Rect buttonRect = new Rect (position.x + (position.width - inspectorButtonAttribute.ButtonWidth) * 0.5f, position.y, inspectorButtonAttribute.ButtonWidth, position.height);
    if (GUI.Button (buttonRect, label.text)) {
      System.Type eventOwnerType = prop.serializedObject.targetObject.GetType ();
      string eventName = inspectorButtonAttribute.MethodName;

      if (_eventMethodInfo == null)
        _eventMethodInfo = eventOwnerType.GetMethod (eventName, BindingFlags.Instance | BindingFlags.Static | BindingFlags.Public | BindingFlags.NonPublic);

      if (_eventMethodInfo != null)
        _eventMethodInfo.Invoke (prop.serializedObject.targetObject, null);
      else
        Debug.LogWarning (string.Format ("InspectorButton: Unable to find method {0} in {1}", eventName, eventOwnerType));
    }
  }
}


unsafe public class PresetController : MonoBehaviour
{
  public bool loadPresetOnStart = false;
  public string presetPath = "Assets/preset.json";
  public string devicePath = "Assets/device.json";


  [InspectorButton ("OnWriteDevice")]
  public bool writeDevice;
  [InspectorButton ("OnReadPreset")]
  public bool readPreset;
  [InspectorButton ("OnWritePreset")]
  public bool writePreset;

  Ossia.Device getDevice ()
  {
    var dev = Controller.Get ();
    Ossia.Device local_device = dev.GetDevice ();
    if (local_device == null) {
      throw new Exception ("LocalDevice is null");
    }
    return local_device;
  }

  void OnReadPreset ()
  {
    string jsontext = System.IO.File.ReadAllText (presetPath);

    Preset p = new Preset (jsontext);

    Ossia.Device dev = getDevice ();
    p.ApplyToDevice (dev, true);

    {
      var objects = UnityEngine.Object.FindObjectsOfType<Ossia.ExposeAttributes> ();
      foreach (Ossia.ExposeAttributes obj in objects) {
        obj.ReceiveUpdates ();
      }
    }
    {
      var exposed = UnityEngine.Object.FindObjectsOfType<Ossia.ExposeComponents> ();
      foreach (Ossia.ExposeComponents obj in exposed) {
        obj.ReceiveUpdates ();
      }
    }
  }

  void OnWritePreset ()
  {
    IntPtr preset;
    BlueYetiAPI.ossia_devices_make_preset (getDevice().GetDevice (), out preset);

    IntPtr str;
    var res = BlueYetiAPI.ossia_presets_write_json (preset, Controller.Get ().appName, out str);

    if (res == ossia_preset_result_enum.OSSIA_PRESETS_OK) {
       System.IO.File.WriteAllText (presetPath, Marshal.PtrToStringAuto (str));
    }
  }

  void OnWriteDevice ()
  {
    var dev = getDevice ();
    IntPtr dev_ptr = dev.GetDevice ();

    IntPtr str;
    var res = BlueYetiAPI.ossia_devices_write_json (dev_ptr, out str);

    if (res == ossia_preset_result_enum.OSSIA_PRESETS_OK) {
       System.IO.File.WriteAllText (devicePath, Marshal.PtrToStringAuto (str));
    }
  }

  // Use this for initialization
  void Start ()
  {
    if (loadPresetOnStart) {
      OnReadPreset ();
    }
  }

  // Update is called once per frame
  void Update ()
  {
  }

}
