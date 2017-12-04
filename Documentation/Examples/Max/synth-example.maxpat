{
	"patcher" : 	{
		"fileversion" : 1,
		"appversion" : 		{
			"major" : 7,
			"minor" : 3,
			"revision" : 5,
			"architecture" : "x86",
			"modernui" : 1
		}
,
		"rect" : [ 34.0, 715.0, 595.0, 301.0 ],
		"bglocked" : 0,
		"openinpresentation" : 0,
		"default_fontsize" : 12.0,
		"default_fontface" : 0,
		"default_fontname" : "Arial",
		"gridonopen" : 1,
		"gridsize" : [ 15.0, 15.0 ],
		"gridsnaponopen" : 1,
		"objectsnaponopen" : 1,
		"statusbarvisible" : 2,
		"toolbarvisible" : 1,
		"lefttoolbarpinned" : 0,
		"toptoolbarpinned" : 0,
		"righttoolbarpinned" : 0,
		"bottomtoolbarpinned" : 0,
		"toolbars_unpinned_last_save" : 0,
		"tallnewobj" : 0,
		"boxanimatetime" : 200,
		"enablehscroll" : 1,
		"enablevscroll" : 1,
		"devicewidth" : 0.0,
		"description" : "",
		"digest" : "",
		"tags" : "",
		"style" : "",
		"subpatcher_template" : "",
		"boxes" : [ 			{
				"box" : 				{
					"id" : "obj-5",
					"maxclass" : "newobj",
					"numinlets" : 0,
					"numoutlets" : 0,
					"patching_rect" : [ 37.0, 223.0, 81.0, 22.0 ],
					"style" : "",
					"text" : "synth synth.4"
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-6",
					"maxclass" : "newobj",
					"numinlets" : 0,
					"numoutlets" : 0,
					"patching_rect" : [ 37.0, 199.0, 81.0, 22.0 ],
					"style" : "",
					"text" : "synth synth.3"
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-2",
					"maxclass" : "newobj",
					"numinlets" : 0,
					"numoutlets" : 0,
					"patching_rect" : [ 37.0, 175.0, 81.0, 22.0 ],
					"style" : "",
					"text" : "synth synth.2"
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-12",
					"maxclass" : "newobj",
					"numinlets" : 1,
					"numoutlets" : 1,
					"outlettype" : [ "" ],
					"patching_rect" : [ 255.0, 74.0, 81.0, 22.0 ],
					"style" : "",
					"text" : "prepend bind"
				}

			}
, 			{
				"box" : 				{
					"activebgcolor" : [ 0.167919, 0.167914, 0.167917, 1.0 ],
					"activebgoncolor" : [ 0.989256, 0.417462, 0.031845, 1.0 ],
					"bordercolor" : [ 0.989256, 0.417462, 0.031845, 1.0 ],
					"focusbordercolor" : [ 0.988235, 0.415686, 0.031373, 0.0 ],
					"fontname" : "Lato Regular",
					"fontsize" : 9.0,
					"id" : "obj-11",
					"maxclass" : "live.tab",
					"multiline" : 0,
					"num_lines_patching" : 1,
					"num_lines_presentation" : 2,
					"numinlets" : 1,
					"numoutlets" : 3,
					"outlettype" : [ "", "", "float" ],
					"parameter_enable" : 1,
					"patching_rect" : [ 255.0, 34.0, 293.0, 26.0 ],
					"saved_attribute_attributes" : 					{
						"valueof" : 						{
							"parameter_enum" : [ "synth.1", "synth.2", "synth.3", "synth.4" ],
							"parameter_longname" : "live.tab",
							"parameter_shortname" : "live.tab",
							"parameter_unitstyle" : 0,
							"parameter_type" : 2
						}

					}
,
					"spacing_x" : 3.0,
					"spacing_y" : 3.0,
					"textcolor" : [ 0.65098, 0.666667, 0.662745, 1.0 ],
					"textoncolor" : [ 0.239216, 0.254902, 0.278431, 1.0 ],
					"varname" : "live.tab"
				}

			}
, 			{
				"box" : 				{
					"bgmode" : 2,
					"border" : 0,
					"clickthrough" : 0,
					"enablehscroll" : 0,
					"enablevscroll" : 0,
					"id" : "obj-10",
					"lockeddragscroll" : 0,
					"maxclass" : "bpatcher",
					"name" : "synth.view.maxpat",
					"numinlets" : 1,
					"numoutlets" : 0,
					"offset" : [ 0.0, 0.0 ],
					"patching_rect" : [ 255.0, 103.0, 293.0, 159.0 ],
					"viewvisibility" : 1
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-9",
					"maxclass" : "message",
					"numinlets" : 2,
					"numoutlets" : 1,
					"outlettype" : [ "" ],
					"patching_rect" : [ 37.0, 64.0, 161.0, 22.0 ],
					"style" : "",
					"text" : "expose oscquery 3455 6789"
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-1",
					"maxclass" : "newobj",
					"numinlets" : 1,
					"numoutlets" : 1,
					"outlettype" : [ "bang" ],
					"patching_rect" : [ 37.0, 34.0, 60.0, 22.0 ],
					"style" : "",
					"text" : "loadbang"
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-3",
					"maxclass" : "newobj",
					"numinlets" : 0,
					"numoutlets" : 0,
					"patching_rect" : [ 37.0, 151.0, 81.0, 22.0 ],
					"style" : "",
					"text" : "synth synth.1"
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-7",
					"maxclass" : "newobj",
					"numinlets" : 1,
					"numoutlets" : 1,
					"outlettype" : [ "" ],
					"patching_rect" : [ 37.0, 88.0, 109.0, 22.0 ],
					"style" : "",
					"text" : "ossia.device synth"
				}

			}
 ],
		"lines" : [ 			{
				"patchline" : 				{
					"destination" : [ "obj-9", 0 ],
					"source" : [ "obj-1", 0 ]
				}

			}
, 			{
				"patchline" : 				{
					"destination" : [ "obj-12", 0 ],
					"midpoints" : [ 401.5, 66.5, 264.5, 66.5 ],
					"source" : [ "obj-11", 1 ]
				}

			}
, 			{
				"patchline" : 				{
					"destination" : [ "obj-10", 0 ],
					"source" : [ "obj-12", 0 ]
				}

			}
, 			{
				"patchline" : 				{
					"destination" : [ "obj-7", 0 ],
					"source" : [ "obj-9", 0 ]
				}

			}
 ],
		"parameters" : 		{
			"obj-10::obj-2" : [ "Cutoff", "Cutoff", 0 ],
			"obj-10::obj-6" : [ "Pan", "Pan", 0 ],
			"obj-10::obj-8" : [ "Ratio", "Ratio", 0 ],
			"obj-10::obj-4" : [ "Gain", "Gain", 0 ],
			"obj-10::obj-1" : [ "Carrier frequency", "Carrier frequency", 0 ],
			"obj-10::obj-7" : [ "live.numbox", "live.numbox", 0 ],
			"obj-10::obj-3" : [ "Index", "Index", 0 ],
			"obj-11" : [ "live.tab", "live.tab", 0 ]
		}
,
		"dependency_cache" : [ 			{
				"name" : "synth.maxpat",
				"bootpath" : "~/@Developments/Git_repositories/i-score/Documentation/Examples/Max",
				"patcherrelativepath" : ".",
				"type" : "JSON",
				"implicit" : 1
			}
, 			{
				"name" : "synth.view.maxpat",
				"bootpath" : "~/@Developments/Git_repositories/i-score/Documentation/Examples/Max",
				"patcherrelativepath" : ".",
				"type" : "JSON",
				"implicit" : 1
			}
, 			{
				"name" : "ossia.device.mxo",
				"type" : "iLaX"
			}
, 			{
				"name" : "ossia.model.mxo",
				"type" : "iLaX"
			}
, 			{
				"name" : "ossia.parameter.mxo",
				"type" : "iLaX"
			}
, 			{
				"name" : "ossia.view.mxo",
				"type" : "iLaX"
			}
, 			{
				"name" : "ossia.remote.mxo",
				"type" : "iLaX"
			}
 ],
		"autosave" : 0
	}

}
