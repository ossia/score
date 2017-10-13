using UnityEngine;
using System.Collections.Generic;
using System.Runtime;
using System.Runtime.InteropServices;
using System;
using Ossia;

namespace Namespace {

	unsafe internal class BlueYetiAPI {


		/* Preset handling	*/

	   /// <summary>
	   /// Read a JSON string to build a preset
	   /// </summary>
	   /// <returns>The result code for the operation.</returns>
	   /// <param name="str">A string containing the JSON.</param>
	   /// <param name="preset">A pointer to a preset that will receive the preset built from JSON.</param>

		[DllImport ("ossia")]
		public static extern ossia_preset_result_enum ossia_presets_read_json (string str, IntPtr * preset);

		/// <summary>
		/// Free a preset
		/// </summary>
		/// <returns>The result code for the operation.</returns>
		/// <param name="preset">The preset to free.</param>

		[DllImport ("ossia")]
		public static extern ossia_preset_result_enum ossia_presets_free (IntPtr preset);

		/// <summary>
		/// Write a JSON string from a preset
		/// </summary>
		/// <returns>The result code for the operation.</returns>
		/// <param name="preset">A preset containing data to convert to JSON.</param>
		/// <param name="buffer">A string buffer receiving the resulting JSON.</param>

		[DllImport ("ossia")]
		public static extern ossia_preset_result_enum ossia_presets_write_json (IntPtr preset, string device, out IntPtr buffer);


		/// <summary>
		/// Get the number of elements in a preset
		/// </summary>
		/// <returns>The result code for the operation.</returns>
		/// <param name="preset">The target preset.</param>
		/// <param name="sizebuffer">A pointer to an int buffer that will receive the size.</param>

		[DllImport ("ossia")]
		public static extern ossia_preset_result_enum ossia_presets_size(IntPtr preset, int* sizebuffer);

		/// <summary>
		/// Convert the preset to a compact string format
		/// </summary>
		/// <returns>The result code for the operation.</returns>
		/// <param name="preset">The target preset.</param>
		/// <param name="buffer">A pointer to a string buffer receiving the resulting string.</param>

		[DllImport ("ossia")]
		public static extern ossia_preset_result_enum ossia_presets_to_string (IntPtr preset, IntPtr* buffer);

		/* Devices handling */

		/// <summary>
		/// Read a JSON file to fill a device
		/// </summary>
		/// <returns>The result code for the operation.</returns>
		/// <param name="ossia_device">A pointer to a device that will be filled with the JSON data</param>
		/// <param name="str">A string containing the JSON data</param>

		[DllImport ("ossia")]
		public static extern ossia_preset_result_enum ossia_devices_read_json (IntPtr * ossia_device, string str);

		/// <summary>
		/// Write a JSON string corresponding to the device's structure
		/// </summary>
		/// <returns>The result code for the operation.</returns>
		/// <param name="ossia_device">The device to convert to JSON.</param>
		/// <param name="buffer">A pointer to a string buffer which will receive the resulting JSON.</param>

		[DllImport ("ossia")]
		public static extern ossia_preset_result_enum ossia_devices_write_json (IntPtr ossia_device, out IntPtr buffer);

		/// <summary>
		/// Apply a preset to a device
		/// </summary>
		/// <returns>The result code for the operation.</returns>
		/// <param name="ossia_device">The device to fill with the nodes specified in the preset.</param>
		/// <param name="preset">The preset to apply.</param>
		/// <param name="keep_arch">If set to <c>true</c>, keeps device architecture intact (<c>true</c> by default).</param>

		[DllImport ("ossia")]
		public static extern ossia_preset_result_enum ossia_devices_apply_preset (IntPtr ossia_device, IntPtr preset, bool keep_arch);

		/// <summary>
		/// Make a preset from a device
		/// </summary>
		/// <returns>The result code for the operation.</returns>
		/// <param name="ossia_device">The device to make into a preset.</param>
		/// <param name="preset">A pointer to a preset buffer that will contain the resulting preset.</param>

		[DllImport ("ossia")]
		public static extern ossia_preset_result_enum ossia_devices_make_preset (IntPtr ossia_device, out IntPtr preset);

		/// <summary>
		/// Convert a device into a compact string
		/// </summary>
		/// <returns>The result code for the operation.</returns>
		/// <param name="ossia_device">The device to convert into a string.</param>
		/// <param name="buffer">A pointer to a string buffer that will contain the resulting string.</param>

		[DllImport ("ossia")]
		public static extern ossia_preset_result_enum ossia_devices_to_string (IntPtr ossia_device, IntPtr* buffer);

		/// <summary>
		/// Set the debug log
		/// </summary>
		/// <param name="fp">Fp.</param>

		[DllImport ("ossia")]
		public static extern void ossia_preset_set_debug_logger( IntPtr fp );

		/* Miscellaneous */

		/// <summary>
		/// Get a node from its path in the device
		/// </summary>
		/// <returns>The result code for the operation.</returns>
		/// <param name="ossia_device">The device in which to search.</param>
		/// <param name="nodekeys">The path to the node in the device.</param>
		/// <param name="nodebuffer">A pointer to a node buffer that will hold the resulting node.</param>

		[DllImport ("ossia")]
		public static extern ossia_preset_result_enum ossia_devices_get_node (IntPtr ossia_device, string nodekeys, IntPtr* nodebuffer);

		/// <summary>
		/// Get a child node by its name
		/// </summary>
		/// <returns>The result code for the operation.</returns>
		/// <param name="rootnode">The parent of the child node.</param>
		/// <param name="childname">The name of the child node.</param>
		/// <param name="nodebuffer">A pointer to a node buffer that will hold the resulting node.</param>

		[DllImport ("ossia")]
		public static extern ossia_preset_result_enum ossia_devices_get_child (IntPtr rootnode, string childname, IntPtr* nodebuffer);

		public static Ossia.Node GetChildFromName(Ossia.Node root, string childname) {
			IntPtr childptr;
			ossia_preset_result_enum code;
			code = ossia_devices_get_child (root.GetNode (), "/" + root.GetName() + "/" + childname, &childptr);
			if (code != ossia_preset_result_enum.OSSIA_PRESETS_OK) {
				throw new Exception ("Error code " + code);
			}
			return new Ossia.Node (childptr);
		}

		/// <summary>
		/// Free a string
		/// </summary>
		/// <returns>The result code for the operation.</returns>
		/// <param name="str">The string to free.</param>

		[DllImport ("ossia")]
		public static extern ossia_preset_result_enum ossia_preset_free_string (IntPtr str);

		[DllImport ("ossia")]
		public static extern ossia_preset_result_enum ossia_devices_get_node_parameter (IntPtr node, IntPtr* addr);

		public static Ossia.Parameter GetNodeParameter(Ossia.Node node) {
			IntPtr addr;
			ossia_preset_result_enum code;
			code = ossia_devices_get_node_parameter (node.GetNode (), &addr);
			if (code != ossia_preset_result_enum.OSSIA_PRESETS_OK) {
				throw new Exception ("Error code " + code);
			}
			return new Ossia.Parameter (addr);
		}
	}

	unsafe public class Preset {

		internal IntPtr preset;

		public Preset() {}

		public Preset(string json) {
			fixed (IntPtr* presetptr = &preset) {
				ossia_preset_result_enum code = BlueYetiAPI.ossia_presets_read_json (json, presetptr);
				if (code != ossia_preset_result_enum.OSSIA_PRESETS_OK) {
					throw new Exception ("Error code " + code);
				}
			}
		}
    public string WriteJson(string device) {
			IntPtr ptr;
			ossia_preset_result_enum code = BlueYetiAPI.ossia_presets_write_json (preset, device, out ptr);
			if (code == ossia_preset_result_enum.OSSIA_PRESETS_OK) {
				string str = Marshal.PtrToStringAuto (ptr);
				Debug.Log ("Wrote json \"" + str + "\"");
				BlueYetiAPI.ossia_preset_free_string (ptr);
				return str;
			} else {
				throw new Exception ("Error code " + code);
			}
		}

		public override System.String ToString() {
			IntPtr strptr;
			ossia_preset_result_enum code = BlueYetiAPI.ossia_presets_to_string (preset, &strptr);
			if (code == ossia_preset_result_enum.OSSIA_PRESETS_OK) {
				System.String str = Marshal.PtrToStringAuto (strptr);
				BlueYetiAPI.ossia_preset_free_string (strptr);
				return str;
			} else {
				throw new Exception ("Error code " + code);
			}
		}

		public int Size() {
			if (IsNull()) {
				return -1;
			}
			else {
				int s;
				ossia_preset_result_enum code = BlueYetiAPI.ossia_presets_size (preset, &s);
				if (code == ossia_preset_result_enum.OSSIA_PRESETS_OK) {
					return s;
				} else {
					throw new Exception ("Error code " + code);
				}
			}
		}

		public bool IsNull() {
			return preset == IntPtr.Zero;
		}

		public void ApplyToDevice(Ossia.Device dev, bool KeepArch) {
			if (dev.GetDevice() != IntPtr.Zero) {
				//Debug.Log (dev.GetDevice ());
				ossia_preset_result_enum code = ossia_preset_result_enum.OSSIA_PRESETS_OK;
				code = BlueYetiAPI.ossia_devices_apply_preset (dev.GetDevice(), preset, KeepArch);
				if (code != ossia_preset_result_enum.OSSIA_PRESETS_OK) {
					throw new Exception ("Error code " + code);
				}
			} else {
				throw new Exception ("Can't apply preset to null device");
			}
		}

		public void Free () {
			if (!IsNull ()) {
				ossia_preset_result_enum code = BlueYetiAPI.ossia_presets_free (preset);
				if (code != ossia_preset_result_enum.OSSIA_PRESETS_OK) {
					throw new Exception ("Error code " + code);
				} else {
					Debug.Log ("Freed preset");
				}
			}
		}
	}
}
