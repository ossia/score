using System.Runtime;
using System.Runtime.InteropServices;
using System.Collections.Generic;
using System.Linq;
using System;
using UnityEngine;
namespace Ossia
{
  public partial class Value
  {
    public Value()
    { ossia_value = Network.ossia_value_create_impulse(); }
    public Value(bool v)
    { ossia_value = Network.ossia_value_create_bool(v ? 1 : 0); }
    public Value(int v)
    { ossia_value = Network.ossia_value_create_int(v); }
    public Value(float v)
    { ossia_value = Network.ossia_value_create_float(v); }
    public Value(float v, float v2)
    { ossia_value = Network.ossia_value_create_2f(v, v2); }
    public Value(float v, float v2, float v3)
    { ossia_value = Network.ossia_value_create_3f(v, v2, v3); }
    public Value(float v, float v2, float v3, float v4)
    { ossia_value = Network.ossia_value_create_4f(v, v2, v3, v4); }
    public Value(char v)
    { ossia_value = Network.ossia_value_create_char(v); }
    public Value(string v)
    { ossia_value = Network.ossia_value_create_string(v); }
    public Value(IList<float> v)
    {
      var arr = v as float[] ?? v.ToArray();
      ossia_value = Network.ossia_value_create_fn(arr, new UIntPtr((ulong)arr.Length));
    }
    public Value(IList<int> v)
    {
      var arr = v as int[] ?? v.ToArray();
      ossia_value = Network.ossia_value_create_in(arr, new UIntPtr((ulong)arr.Length));
    }
    public Value(IList<Value> v)
    {
      IntPtr[] arr = new IntPtr[v.Count()];
      int i = 0;
      foreach(Value val in v)
        arr[i++] = val.ossia_value;
      ossia_value = Network.ossia_value_create_list(arr, new UIntPtr((ulong)arr.Length));
    }
    public Value(object obj)
    {
      if (obj is int) {
        ossia_value = Network.ossia_value_create_int ((int)obj);
      } else if (obj is bool) {
        ossia_value = Network.ossia_value_create_bool (((bool)obj) ? 1 : 0);
      } else if (obj is float) {
        ossia_value = Network.ossia_value_create_float ((float)obj);
      } else if (obj is double) {
        ossia_value = Network.ossia_value_create_float ((float)((double)obj));
      } else if (obj is char) {
        ossia_value = Network.ossia_value_create_char ((char)obj);
      } else if (obj is string) {
        ossia_value = Network.ossia_value_create_string ((string)obj);
      } else if (obj is IList<float>) {
        var lst = (IList<float>)obj;
        var arr = lst as float[] ?? lst.ToArray ();
        ossia_value = Network.ossia_value_create_fn (arr, new UIntPtr ((ulong)arr.Length));
      } else if (obj is IList<int>) {
        var lst = (IList<int>)obj;
        var arr = lst as int[] ?? lst.ToArray ();
        ossia_value = Network.ossia_value_create_in (arr, new UIntPtr ((ulong)arr.Length));
      } else if (obj is IList<Value>) {
        var lst = (IList<Value>)obj;
        IntPtr[] arr = new IntPtr[lst.Count ()];
        int i = 0;
        foreach (Value val in lst)
          arr [i++] = val.ossia_value;
        ossia_value = Network.ossia_value_create_list (arr, new UIntPtr ((ulong)arr.Length));
      } else if (obj is System.Int32) {
        ossia_value = Network.ossia_value_create_int ((int)obj);
      } else if (obj is System.Boolean) {
        ossia_value = Network.ossia_value_create_bool (((bool)obj) ? 1 : 0);
      } else if (obj is System.Single) {
        ossia_value = Network.ossia_value_create_float ((float)obj);
      } else if (obj is System.Double) {
        ossia_value = Network.ossia_value_create_float ((float)obj);
      } else if (obj is UnityEngine.Vector2) {
        var vec = (Vector2)obj;
        ossia_value = Network.ossia_value_create_2f (vec.x, vec.y);
      } else if (obj is UnityEngine.Vector3) {
        var vec = (Vector3)obj;
        ossia_value = Network.ossia_value_create_3f (vec.x, vec.y, vec.z);
      } else if (obj is UnityEngine.Vector4) {
        var vec = (Vector4)obj;
        ossia_value = Network.ossia_value_create_4f (vec.w, vec.x, vec.y, vec.z);
      } else if (obj is UnityEngine.Quaternion) {
        var vec = (Quaternion)obj;
        ossia_value = Network.ossia_value_create_4f (vec.w, vec.x, vec.y, vec.z);
      } else if (obj is UnityEngine.Color) {
        var vec = (Color)obj;
        ossia_value = Network.ossia_value_create_4f (vec.r, vec.g, vec.b, vec.a);
      } else if (obj is UnityEngine.Color32) {
        var vec = (Color)obj;
        ossia_value = Network.ossia_value_create_4f (vec.r, vec.g, vec.b, vec.a);
      } else if (obj is System.String) {
        ossia_value = Network.ossia_value_create_string ((string)obj);
      } else {
        throw new Exception("unimplemented type: " + obj.GetType());
      }

    }

    static public ossia_type GetOssiaType(Type obj)
    {
      if (obj == typeof(System.Int32))
      {
        return ossia_type.INT;
      }
      else if (obj == typeof(System.Boolean))
      {
        return ossia_type.BOOL;
      }
      else if (obj == typeof(System.Single))
      {
        return ossia_type.FLOAT;
      }
      else if (obj == typeof(System.Double))
      {
        return ossia_type.FLOAT;
      }
      else if (obj == typeof(System.Char))
      {
        return ossia_type.CHAR;
      }
      else if (obj == typeof(System.String))
      {
        return ossia_type.STRING;
      }
      else if (obj == typeof(float[]))
      {
        return ossia_type.LIST;
      }
      else if (obj == typeof(int[]))
      {
        return ossia_type.LIST;
      }
      else if (obj == typeof(vec2f))
      {
        return ossia_type.VEC2F;
      }
      else if (obj == typeof(vec3f))
      {
        return ossia_type.VEC3F;
      }
      else if (obj == typeof(vec4f))
      {
        return ossia_type.VEC4F;
      }
      else if (obj == typeof(UnityEngine.Vector2))
      {
        return ossia_type.VEC2F;
      }
      else if (obj == typeof(UnityEngine.Vector3))
      {
        return ossia_type.VEC3F;
      }
      else if (obj == typeof(UnityEngine.Vector4))
      {
        return ossia_type.VEC4F;
      }
      else if (obj == typeof(UnityEngine.Quaternion))
      {
        return ossia_type.VEC4F;
      }
      else if (obj == typeof(UnityEngine.Color))
      {
        return ossia_type.VEC4F;
      }
      else if (obj == typeof(UnityEngine.Color32))
      {
        return ossia_type.VEC4F;
      }

      throw new Exception("unimplemented type" + obj.GetType());
    }


    public object Get(Type obj)
    {
      ossia_type t = GetOssiaType ();
      switch (t) {
      case ossia_type.FLOAT:
        {
          if (obj == typeof(float))
            return GetFloat ();
          else if (obj == typeof(double))
            return (double)GetFloat ();
          else if (obj == typeof(System.Int32))
            return (System.Int32)GetFloat ();
          else if (obj == typeof(System.Int64))
            return (System.Int64)GetFloat ();
          break;
        }
      case ossia_type.INT:
        {
          if (obj == typeof(float))
            return GetInt ();
          else if (obj == typeof(double))
            return (double)GetInt ();
          else if (obj == typeof(System.Int32))
            return (System.Int32)GetInt ();
          else if (obj == typeof(System.Int64))
            return (System.Int64)GetInt ();
          break;
        }
      case ossia_type.VEC2F:
        {
          var vec = GetVec2f ();
          if (obj == typeof(Vector2))
            return vec;
          break;
        }
      case ossia_type.VEC3F:
        {
          var vec = GetVec3f ();
          if (obj == typeof(Vector3))
            return vec;
          break;
        }
      case ossia_type.VEC4F:
        {
          var vec = GetVec4f ();
          if (obj == typeof(Vector4))
            return vec;
          else if(obj == typeof(Quaternion))
            return new Quaternion(vec.x, vec.y, vec.z, vec.w);
          else if(obj == typeof(Color))
            return new Color(vec.x, vec.y, vec.z, vec.w);
          else if(obj == typeof(Color32))
            return new Color32((byte)vec.x, (byte)vec.y, (byte)vec.z, (byte)vec.w);
          break;
        }
      case ossia_type.BOOL:
        return GetBool ();
      case ossia_type.STRING:
        return GetString ();
      case ossia_type.LIST:
        return GetList ();
      case ossia_type.CHAR:
        return GetChar ();
      }
      return null;
    }

    public ossia_type GetOssiaType()
    {
      return Network.ossia_value_get_type(ossia_value);
    }

    public int GetInt()
    {
      return Network.ossia_value_to_int(ossia_value);
    }
    public bool GetBool()
    {
      return Network.ossia_value_to_bool(ossia_value);
    }
    public float GetFloat()
    {
      return Network.ossia_value_to_float(ossia_value);
    }
    public Vector2 GetVec2f()
    {
      vec2f val = Network.ossia_value_to_2f(ossia_value);
      return new Vector2 (val.f1, val.f2);
    }
    public Vector3 GetVec3f()
    {
      vec3f val = Network.ossia_value_to_3f(ossia_value);
      return new Vector3 (val.f1, val.f2, val.f3);
    }
    public Vector4 GetVec4f()
    {
      vec4f val = Network.ossia_value_to_4f(ossia_value);
      return new Vector4 (val.f1, val.f2, val.f3, val.f4);
    }
    public char GetChar()
    {
      return Network.ossia_value_to_char(ossia_value);
    }

    public int[] GetIntArray()
    {
      IntPtr data = IntPtr.Zero;
      UIntPtr sz = UIntPtr.Zero;
      Network.ossia_value_to_in(ossia_value, out data, out sz);

      int[] v = new int[(int) sz];
      Marshal.Copy (data, v, 0, (int)sz);

      Network.ossia_value_free_in (data);
      return v;
    }

    public float[] GetFloatArray()
    {
      IntPtr data = IntPtr.Zero;
      UIntPtr sz = UIntPtr.Zero;
      Network.ossia_value_to_fn(ossia_value, out data, out sz);

      int size = (int)sz.ToUInt64 ();
      float[] v = new float[size];
      Marshal.Copy (data, v, 0, size);

      Network.ossia_value_free_fn (data);
      return v;
    }

    public unsafe Value[] GetList()
    {
      IntPtr data = IntPtr.Zero;
      UIntPtr sz = UIntPtr.Zero;
      Network.ossia_value_to_fn(ossia_value, out data, out sz);

      Value[] v = new Value[(int) sz];
      IntPtr* ptr = (IntPtr*)data.ToPointer ();
      for (int i = 0; i < (int)sz; i++) {
        IntPtr p = *ptr;
        v [i] = new Value (p);
        ptr = ptr + Marshal.SizeOf (sz);
      }

      Network.ossia_value_free_fn (data);
      return v;
    }

    public string GetString()
    {
      IntPtr str = IntPtr.Zero;
      UIntPtr sz = UIntPtr.Zero;
      Network.ossia_value_to_byte_array(ossia_value, out str, out sz);

      string s = Marshal.PtrToStringAnsi (str, (int) sz);

      Network.ossia_string_free (str);
      return s;
    }

    public object Get()
    {
      ossia_type t = GetOssiaType ();
      switch (t) {
      case ossia_type.FLOAT:
        return GetFloat ();
      case ossia_type.INT:
        return GetInt ();
      case ossia_type.VEC2F:
        return GetVec2f ();
      case ossia_type.VEC3F:
        return GetVec3f ();
      case ossia_type.VEC4F:
        return GetVec4f ();
      case ossia_type.BOOL:
        return GetBool ();
      case ossia_type.STRING:
        return GetString ();
      case ossia_type.LIST:
        return GetList ();
      case ossia_type.CHAR:
        return GetChar ();
      }
      return null;
    }


    public int ConvertInt()
    {
      return Network.ossia_value_convert_int(ossia_value);
    }
    public bool ConvertBool()
    {
      return Network.ossia_value_convert_bool(ossia_value);
    }
    public float ConvertFloat()
    {
      return Network.ossia_value_convert_float(ossia_value);
    }
    public char ConvertChar()
    {
      return Network.ossia_value_convert_char(ossia_value);
    }
    public string ConvertString()
    {
      IntPtr str = IntPtr.Zero;
      UIntPtr sz = UIntPtr.Zero;
      Network.ossia_value_convert_byte_array(ossia_value, out str, out sz);

      string s = Marshal.PtrToStringAnsi (str, (int) sz);

      Network.ossia_string_free (str);
      return s;
    }

    internal readonly IntPtr ossia_value = IntPtr.Zero;
    internal protected Value(IntPtr v)
    {
      ossia_value = v;
    }

    ~Value()
    {
      Network.ossia_value_free(ossia_value);
    }
  }
}
