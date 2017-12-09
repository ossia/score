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
		"rect" : [ 572.0, 56.0, 536.0, 960.0 ],
		"bglocked" : 0,
		"openinpresentation" : 1,
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
					"fontface" : 1,
					"fontsize" : 10.0,
					"id" : "obj-12",
					"maxclass" : "comment",
					"numinlets" : 1,
					"numoutlets" : 0,
					"patching_rect" : [ 358.0, 275.0, 133.0, 18.0 ],
					"presentation" : 1,
					"presentation_rect" : [ 199.0, 67.0, 38.0, 18.0 ],
					"style" : "",
					"text" : "Ramp"
				}

			}
, 			{
				"box" : 				{
					"comment" : "",
					"id" : "obj-10",
					"index" : 0,
					"maxclass" : "inlet",
					"numinlets" : 0,
					"numoutlets" : 1,
					"outlettype" : [ "" ],
					"patching_rect" : [ 813.0, 64.0, 30.0, 30.0 ],
					"style" : ""
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-3",
					"maxclass" : "live.dial",
					"numinlets" : 1,
					"numoutlets" : 2,
					"outlettype" : [ "", "float" ],
					"parameter_enable" : 1,
					"patching_rect" : [ 571.0, 223.5, 44.0, 47.0 ],
					"presentation" : 1,
					"presentation_rect" : [ 239.0, 9.0, 44.0, 47.0 ],
					"prototypename" : "amount",
					"saved_attribute_attributes" : 					{
						"valueof" : 						{
							"parameter_linknames" : 1,
							"parameter_initial_enable" : 1,
							"parameter_longname" : "Index",
							"parameter_initial" : [ 0 ],
							"parameter_shortname" : "Index",
							"parameter_unitstyle" : 5,
							"parameter_type" : 0,
							"parameter_mmax" : 1000.0,
							"parameter_speedlim" : 0.0
						}

					}
,
					"varname" : "Index"
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-8",
					"maxclass" : "live.dial",
					"numinlets" : 1,
					"numoutlets" : 2,
					"outlettype" : [ "", "float" ],
					"parameter_enable" : 1,
					"patching_rect" : [ 374.0, 81.5, 44.0, 47.0 ],
					"presentation" : 1,
					"presentation_rect" : [ 177.0, 9.0, 44.0, 47.0 ],
					"prototypename" : "amount",
					"saved_attribute_attributes" : 					{
						"valueof" : 						{
							"parameter_linknames" : 1,
							"parameter_initial_enable" : 1,
							"parameter_longname" : "Ratio",
							"parameter_initial" : [ 0 ],
							"parameter_shortname" : "Ratio",
							"parameter_unitstyle" : 5,
							"parameter_type" : 0,
							"parameter_mmin" : 0.001,
							"parameter_mmax" : 10.0,
							"parameter_speedlim" : 0.0
						}

					}
,
					"varname" : "Ratio"
				}

			}
, 			{
				"box" : 				{
					"appearance" : 2,
					"id" : "obj-7",
					"maxclass" : "live.numbox",
					"numinlets" : 1,
					"numoutlets" : 2,
					"outlettype" : [ "", "float" ],
					"parameter_enable" : 1,
					"patching_rect" : [ 337.0, 236.0, 43.0, 15.0 ],
					"presentation" : 1,
					"presentation_rect" : [ 239.0, 67.0, 43.0, 15.0 ],
					"prototypename" : "time",
					"saved_attribute_attributes" : 					{
						"valueof" : 						{
							"parameter_linknames" : 1,
							"parameter_initial_enable" : 1,
							"parameter_longname" : "live.numbox",
							"parameter_initial" : [ 0 ],
							"parameter_shortname" : "live.numbox",
							"parameter_unitstyle" : 2,
							"parameter_type" : 0,
							"parameter_steps" : 41,
							"parameter_mmax" : 1000.0,
							"parameter_speedlim" : 0.0
						}

					}
,
					"textcolor" : [ 0.101961, 0.121569, 0.172549, 1.0 ],
					"varname" : "live.numbox"
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-6",
					"maxclass" : "live.dial",
					"numinlets" : 1,
					"numoutlets" : 2,
					"outlettype" : [ "", "float" ],
					"parameter_enable" : 1,
					"patching_rect" : [ 166.5, 614.5, 44.0, 53.0 ],
					"presentation" : 1,
					"presentation_rect" : [ 239.0, 99.0, 44.0, 53.0 ],
					"prototypename" : "pan",
					"saved_attribute_attributes" : 					{
						"valueof" : 						{
							"parameter_linknames" : 1,
							"parameter_initial_enable" : 1,
							"parameter_longname" : "Pan",
							"parameter_initial" : [ 0 ],
							"parameter_shortname" : "Pan",
							"parameter_unitstyle" : 6,
							"parameter_type" : 0,
							"parameter_mmin" : -1.0,
							"parameter_mmax" : 1.0,
							"parameter_speedlim" : 0.0
						}

					}
,
					"triangle" : 1,
					"varname" : "Pan"
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-4",
					"maxclass" : "live.dial",
					"numinlets" : 1,
					"numoutlets" : 2,
					"outlettype" : [ "", "float" ],
					"parameter_enable" : 1,
					"patching_rect" : [ 227.0, 424.5, 44.0, 47.0 ],
					"presentation" : 1,
					"presentation_rect" : [ 177.0, 99.0, 44.0, 47.0 ],
					"prototypename" : "gain",
					"saved_attribute_attributes" : 					{
						"valueof" : 						{
							"parameter_linknames" : 1,
							"parameter_initial_enable" : 1,
							"parameter_longname" : "Gain",
							"parameter_initial" : [ 0 ],
							"parameter_shortname" : "Gain",
							"parameter_unitstyle" : 4,
							"parameter_type" : 0,
							"parameter_mmin" : -70.0,
							"parameter_mmax" : 0.0,
							"parameter_speedlim" : 0.0
						}

					}
,
					"varname" : "Gain"
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-2",
					"maxclass" : "live.slider",
					"numinlets" : 1,
					"numoutlets" : 2,
					"orientation" : 1,
					"outlettype" : [ "", "float" ],
					"parameter_enable" : 1,
					"patching_rect" : [ 53.0, 258.0, 50.0, 40.0 ],
					"presentation" : 1,
					"presentation_rect" : [ 6.0, 99.0, 153.0, 40.0 ],
					"prototypename" : "freq",
					"saved_attribute_attributes" : 					{
						"valueof" : 						{
							"parameter_linknames" : 1,
							"parameter_initial_enable" : 1,
							"parameter_longname" : "Cutoff",
							"parameter_initial" : [ 0 ],
							"parameter_shortname" : "Cutoff",
							"parameter_unitstyle" : 3,
							"parameter_type" : 0,
							"parameter_exponent" : 4.0,
							"parameter_mmax" : 10000.0,
							"parameter_speedlim" : 0.0
						}

					}
,
					"varname" : "Cutoff"
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-1",
					"maxclass" : "live.slider",
					"numinlets" : 1,
					"numoutlets" : 2,
					"orientation" : 1,
					"outlettype" : [ "", "float" ],
					"parameter_enable" : 1,
					"patching_rect" : [ 53.0, 64.0, 50.0, 40.0 ],
					"presentation" : 1,
					"presentation_rect" : [ 6.0, 16.0, 153.0, 40.0 ],
					"prototypename" : "freq",
					"saved_attribute_attributes" : 					{
						"valueof" : 						{
							"parameter_linknames" : 1,
							"parameter_initial_enable" : 1,
							"parameter_longname" : "Carrier frequency",
							"parameter_initial" : [ 0 ],
							"parameter_shortname" : "Carrier frequency",
							"parameter_unitstyle" : 3,
							"parameter_type" : 0,
							"parameter_exponent" : 4.0,
							"parameter_mmax" : 10000.0,
							"parameter_speedlim" : 0.0
						}

					}
,
					"varname" : "Carrier frequency"
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-5",
					"maxclass" : "newobj",
					"numinlets" : 1,
					"numoutlets" : 3,
					"outlettype" : [ "", "", "" ],
					"patching_rect" : [ 447.0, 101.0, 168.0, 22.0 ],
					"style" : "",
					"text" : "ossia.remote modulation/ratio"
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-9",
					"maxclass" : "newobj",
					"numinlets" : 1,
					"numoutlets" : 3,
					"outlettype" : [ "", "", "" ],
					"patching_rect" : [ 102.0, 101.0, 173.0, 22.0 ],
					"style" : "",
					"text" : "ossia.remote carrier/frequency"
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-19",
					"maxclass" : "newobj",
					"numinlets" : 1,
					"numoutlets" : 3,
					"outlettype" : [ "", "", "" ],
					"patching_rect" : [ 391.0, 236.0, 133.0, 22.0 ],
					"style" : "",
					"text" : "ossia.remote ramptime"
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-20",
					"maxclass" : "newobj",
					"numinlets" : 1,
					"numoutlets" : 3,
					"outlettype" : [ "", "", "" ],
					"patching_rect" : [ 630.0, 236.0, 173.0, 22.0 ],
					"style" : "",
					"text" : "ossia.remote modulation/index"
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-28",
					"maxclass" : "newobj",
					"numinlets" : 1,
					"numoutlets" : 3,
					"outlettype" : [ "", "", "" ],
					"patching_rect" : [ 289.0, 437.0, 105.0, 22.0 ],
					"style" : "",
					"text" : "ossia.remote gain"
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-40",
					"maxclass" : "newobj",
					"numinlets" : 1,
					"numoutlets" : 3,
					"outlettype" : [ "", "", "" ],
					"patching_rect" : [ 126.0, 270.0, 168.0, 22.0 ],
					"style" : "",
					"text" : "ossia.remote cutoff/frequency"
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-41",
					"maxclass" : "newobj",
					"numinlets" : 1,
					"numoutlets" : 3,
					"outlettype" : [ "", "", "" ],
					"patching_rect" : [ 227.0, 630.0, 103.0, 22.0 ],
					"style" : "",
					"text" : "ossia.remote pan"
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-42",
					"maxclass" : "newobj",
					"numinlets" : 1,
					"numoutlets" : 1,
					"outlettype" : [ "" ],
					"patching_rect" : [ 813.0, 112.0, 108.0, 22.0 ],
					"style" : "",
					"text" : "ossia.view #1"
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
					"destination" : [ "obj-42", 0 ],
					"source" : [ "obj-10", 0 ]
				}

			}
, 			{
				"patchline" : 				{
					"destination" : [ "obj-7", 0 ],
					"source" : [ "obj-19", 0 ]
				}

			}
, 			{
				"patchline" : 				{
					"destination" : [ "obj-40", 0 ],
					"source" : [ "obj-2", 0 ]
				}

			}
, 			{
				"patchline" : 				{
					"destination" : [ "obj-3", 0 ],
					"source" : [ "obj-20", 0 ]
				}

			}
, 			{
				"patchline" : 				{
					"destination" : [ "obj-4", 0 ],
					"source" : [ "obj-28", 0 ]
				}

			}
, 			{
				"patchline" : 				{
					"destination" : [ "obj-20", 0 ],
					"source" : [ "obj-3", 0 ]
				}

			}
, 			{
				"patchline" : 				{
					"destination" : [ "obj-28", 0 ],
					"source" : [ "obj-4", 0 ]
				}

			}
, 			{
				"patchline" : 				{
					"destination" : [ "obj-2", 0 ],
					"source" : [ "obj-40", 0 ]
				}

			}
, 			{
				"patchline" : 				{
					"destination" : [ "obj-6", 0 ],
					"source" : [ "obj-41", 0 ]
				}

			}
, 			{
				"patchline" : 				{
					"destination" : [ "obj-8", 0 ],
					"source" : [ "obj-5", 0 ]
				}

			}
, 			{
				"patchline" : 				{
					"destination" : [ "obj-41", 0 ],
					"source" : [ "obj-6", 0 ]
				}

			}
, 			{
				"patchline" : 				{
					"destination" : [ "obj-19", 0 ],
					"source" : [ "obj-7", 0 ]
				}

			}
, 			{
				"patchline" : 				{
					"destination" : [ "obj-5", 0 ],
					"source" : [ "obj-8", 0 ]
				}

			}
, 			{
				"patchline" : 				{
					"destination" : [ "obj-1", 0 ],
					"source" : [ "obj-9", 0 ]
				}

			}
 ],
		"parameters" : 		{
			"obj-1" : [ "Carrier frequency", "Carrier frequency", 0 ],
			"obj-8" : [ "Ratio", "Ratio", 0 ],
			"obj-7" : [ "live.numbox", "live.numbox", 0 ],
			"obj-6" : [ "Pan", "Pan", 0 ],
			"obj-4" : [ "Gain", "Gain", 0 ],
			"obj-2" : [ "Cutoff", "Cutoff", 0 ],
			"obj-3" : [ "Index", "Index", 0 ]
		}
,
		"dependency_cache" : [ 			{
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
