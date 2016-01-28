{
	"patcher" : 	{
		"fileversion" : 1,
		"appversion" : 		{
			"major" : 7,
			"minor" : 0,
			"revision" : 6,
			"architecture" : "x86",
			"modernui" : 1
		}
,
		"rect" : [ 36.0, 79.0, 315.0, 686.0 ],
		"bglocked" : 0,
		"openinpresentation" : 0,
		"default_fontsize" : 10.0,
		"default_fontface" : 0,
		"default_fontname" : "Verdana",
		"gridonopen" : 1,
		"gridsize" : [ 5.0, 5.0 ],
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
					"hidden" : 1,
					"id" : "obj-8",
					"maxclass" : "message",
					"numinlets" : 2,
					"numoutlets" : 1,
					"outlettype" : [ "" ],
					"patching_rect" : [ 148.0, 5.0, 44.0, 21.0 ],
					"style" : "",
					"text" : "print 1"
				}

			}
, 			{
				"box" : 				{
					"bubble" : 1,
					"bubbleside" : 2,
					"fontname" : "Arial",
					"fontsize" : 13.0,
					"id" : "obj-11",
					"linecount" : 2,
					"maxclass" : "comment",
					"numinlets" : 1,
					"numoutlets" : 0,
					"patching_rect" : [ 76.5, 156.0, 159.0, 55.0 ],
					"style" : "",
					"text" : "Interface to control and watch engine parameters"
				}

			}
, 			{
				"box" : 				{
					"bubble" : 1,
					"fontname" : "Arial",
					"fontsize" : 13.0,
					"id" : "obj-10",
					"maxclass" : "comment",
					"numinlets" : 1,
					"numoutlets" : 0,
					"patching_rect" : [ 138.0, 114.5, 136.0, 25.0 ],
					"style" : "",
					"text" : "Core audio engine"
				}

			}
, 			{
				"box" : 				{
					"bubble" : 1,
					"fontname" : "Arial",
					"fontsize" : 13.0,
					"id" : "obj-21",
					"linecount" : 3,
					"maxclass" : "comment",
					"numinlets" : 1,
					"numoutlets" : 0,
					"patching_rect" : [ 138.0, 25.0, 117.0, 54.0 ],
					"style" : "",
					"text" : "Enable Minuit communication with i-score"
				}

			}
, 			{
				"box" : 				{
					"id" : "obj-1",
					"linecount" : 7,
					"maxclass" : "newobj",
					"numinlets" : 1,
					"numoutlets" : 0,
					"patching_rect" : [ 6.0, 5.0, 128.0, 94.0 ],
					"style" : "",
					"text" : "j.minuit_device @local/name Max @local/port 9998 @local/ip 127.0.0.1 @remote/name i-score @remote/port 13579 @remote/ip 127.0.0.1"
				}

			}
, 			{
				"box" : 				{
					"annotation" : "none",
					"args" : [ "output" ],
					"bgmode" : 0,
					"border" : 0,
					"clickthrough" : 0,
					"enablehscroll" : 0,
					"enablevscroll" : 0,
					"id" : "obj-7",
					"lockeddragscroll" : 0,
					"maxclass" : "bpatcher",
					"name" : "output~.view.maxpat",
					"numinlets" : 1,
					"numoutlets" : 1,
					"offset" : [ 0.0, 0.0 ],
					"outlettype" : [ "" ],
					"patching_rect" : [ 6.0, 506.0, 300.0, 175.0 ],
					"presentation_rect" : [ 0.0, 0.0, 300.0, 175.0 ],
					"viewvisibility" : 1
				}

			}
, 			{
				"box" : 				{
					"annotation" : "none",
					"args" : [ "filter" ],
					"bgmode" : 0,
					"border" : 0,
					"clickthrough" : 0,
					"enablehscroll" : 0,
					"enablevscroll" : 0,
					"id" : "obj-5",
					"lockeddragscroll" : 0,
					"maxclass" : "bpatcher",
					"name" : "filter~.view.maxpat",
					"numinlets" : 1,
					"numoutlets" : 1,
					"offset" : [ 0.0, 0.0 ],
					"outlettype" : [ "" ],
					"patching_rect" : [ 6.0, 394.0, 300.0, 105.0 ],
					"presentation_rect" : [ 0.0, 0.0, 300.0, 105.0 ],
					"viewvisibility" : 1
				}

			}
, 			{
				"box" : 				{
					"annotation" : "none",
					"args" : [ "input" ],
					"bgmode" : 0,
					"border" : 0,
					"clickthrough" : 0,
					"enablehscroll" : 0,
					"enablevscroll" : 0,
					"id" : "obj-4",
					"lockeddragscroll" : 0,
					"maxclass" : "bpatcher",
					"name" : "input~.view.maxpat",
					"numinlets" : 1,
					"numoutlets" : 1,
					"offset" : [ 0.0, 0.0 ],
					"outlettype" : [ "" ],
					"patching_rect" : [ 6.0, 213.0, 300.0, 175.0 ],
					"presentation_rect" : [ 0.0, 0.0, 300.0, 175.0 ],
					"viewvisibility" : 1
				}

			}
, 			{
				"box" : 				{
					"fontname" : "Verdana",
					"fontsize" : 10.0,
					"id" : "obj-6",
					"maxclass" : "newobj",
					"numinlets" : 0,
					"numoutlets" : 0,
					"patcher" : 					{
						"fileversion" : 1,
						"appversion" : 						{
							"major" : 7,
							"minor" : 0,
							"revision" : 6,
							"architecture" : "x86",
							"modernui" : 1
						}
,
						"rect" : [ 420.0, 79.0, 328.0, 336.0 ],
						"bglocked" : 0,
						"openinpresentation" : 0,
						"default_fontsize" : 10.0,
						"default_fontface" : 0,
						"default_fontname" : "Verdana",
						"gridonopen" : 1,
						"gridsize" : [ 5.0, 5.0 ],
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
						"visible" : 1,
						"boxes" : [ 							{
								"box" : 								{
									"id" : "obj-3",
									"maxclass" : "newobj",
									"numinlets" : 3,
									"numoutlets" : 1,
									"outlettype" : [ "int" ],
									"patching_rect" : [ 20.0, 267.0, 40.0, 21.0 ],
									"style" : "",
									"text" : "itoa"
								}

							}
, 							{
								"box" : 								{
									"id" : "obj-2",
									"maxclass" : "newobj",
									"numinlets" : 1,
									"numoutlets" : 2,
									"outlettype" : [ "", "" ],
									"patching_rect" : [ 20.0, 297.0, 138.0, 21.0 ],
									"style" : "",
									"text" : "j.return key @type string"
								}

							}
, 							{
								"box" : 								{
									"id" : "obj-1",
									"maxclass" : "newobj",
									"numinlets" : 0,
									"numoutlets" : 4,
									"outlettype" : [ "int", "int", "int", "int" ],
									"patching_rect" : [ 20.0, 238.0, 40.0, 21.0 ],
									"style" : "",
									"text" : "key"
								}

							}
, 							{
								"box" : 								{
									"annotation" : "A generic audio output model with built in master saturation, limiter, and recording abilities.",
									"id" : "obj-9",
									"maxclass" : "newobj",
									"numinlets" : 2,
									"numoutlets" : 1,
									"outlettype" : [ "" ],
									"patching_rect" : [ 20.0, 126.0, 135.25, 21.0 ],
									"style" : "",
									"text" : "output~.model output"
								}

							}
, 							{
								"box" : 								{
									"annotation" : "stereo 2nd order iir filter based on biquad~",
									"id" : "obj-5",
									"maxclass" : "newobj",
									"numinlets" : 2,
									"numoutlets" : 3,
									"outlettype" : [ "signal", "signal", "" ],
									"patching_rect" : [ 20.0, 81.0, 251.5, 21.0 ],
									"style" : "",
									"text" : "filter~.model filter"
								}

							}
, 							{
								"box" : 								{
									"annotation" : "none",
									"id" : "obj-4",
									"maxclass" : "newobj",
									"numinlets" : 1,
									"numoutlets" : 3,
									"outlettype" : [ "signal", "signal", "" ],
									"patching_rect" : [ 20.0, 32.0, 251.5, 21.0 ],
									"style" : "",
									"text" : "input~.model input"
								}

							}
 ],
						"lines" : [ 							{
								"patchline" : 								{
									"destination" : [ "obj-3", 0 ],
									"disabled" : 0,
									"hidden" : 0,
									"source" : [ "obj-1", 0 ]
								}

							}
, 							{
								"patchline" : 								{
									"destination" : [ "obj-2", 0 ],
									"disabled" : 0,
									"hidden" : 0,
									"source" : [ "obj-3", 0 ]
								}

							}
, 							{
								"patchline" : 								{
									"destination" : [ "obj-5", 1 ],
									"disabled" : 0,
									"hidden" : 0,
									"source" : [ "obj-4", 1 ]
								}

							}
, 							{
								"patchline" : 								{
									"destination" : [ "obj-5", 0 ],
									"disabled" : 0,
									"hidden" : 0,
									"source" : [ "obj-4", 0 ]
								}

							}
, 							{
								"patchline" : 								{
									"destination" : [ "obj-9", 1 ],
									"disabled" : 0,
									"hidden" : 0,
									"source" : [ "obj-5", 1 ]
								}

							}
, 							{
								"patchline" : 								{
									"destination" : [ "obj-9", 0 ],
									"disabled" : 0,
									"hidden" : 0,
									"source" : [ "obj-5", 0 ]
								}

							}
 ]
					}
,
					"patching_rect" : [ 6.0, 116.5, 124.0, 21.0 ],
					"saved_object_attributes" : 					{
						"description" : "",
						"digest" : "",
						"fontname" : "Verdana",
						"fontsize" : 10.0,
						"globalpatchername" : "",
						"style" : "",
						"tags" : ""
					}
,
					"style" : "",
					"text" : "p engine",
					"varname" : "engine"
				}

			}
 ],
		"lines" : [ 			{
				"patchline" : 				{
					"destination" : [ "obj-1", 0 ],
					"disabled" : 0,
					"hidden" : 1,
					"midpoints" : [ 157.5, 30.0, 141.5, 30.0, 141.5, 2.0, 15.5, 2.0 ],
					"source" : [ "obj-8", 0 ]
				}

			}
 ],
		"parameters" : 		{
			"obj-7::obj-111" : [ "live.numbox[7]", "live.numbox[2]", 0 ],
			"obj-4::obj-1::obj-5" : [ "Preamp", "Preamp", 0 ],
			"obj-7::obj-110" : [ "live.numbox[8]", "live.numbox[1]", 0 ],
			"obj-7::obj-12::obj-12" : [ "Lookahead[1]", "Lookahead", 0 ],
			"obj-4::obj-110" : [ "live.numbox[1]", "live.numbox[1]", 0 ],
			"obj-7::obj-12::obj-5" : [ "Preamp[1]", "Preamp", 0 ],
			"obj-4::obj-82" : [ "pan", "Pan", 0 ],
			"obj-7::obj-12::obj-27" : [ "Threshold[1]", "Threshold", 0 ],
			"obj-4::obj-29::obj-48" : [ "live.menu[1]", "live.menu", 0 ],
			"obj-4::obj-1::obj-27" : [ "Threshold", "Threshold", 0 ],
			"obj-4::obj-59" : [ "live.text[8]", "live.text[3]", 0 ],
			"obj-4::obj-47" : [ "live.numbox[3]", "live.numbox", 0 ],
			"obj-4::obj-1::obj-15" : [ "Postamp", "Postamp", 0 ],
			"obj-4::obj-1::obj-13" : [ "Release", "Release", 0 ],
			"obj-5::obj-7" : [ "live.numbox[6]", "live.numbox", 0 ],
			"obj-5::obj-14" : [ "live.numbox[5]", "live.numbox", 0 ],
			"obj-5::obj-19" : [ "live.numbox", "live.numbox", 0 ],
			"obj-4::obj-111" : [ "live.numbox[2]", "live.numbox[2]", 0 ],
			"obj-7::obj-103" : [ "live.text[16]", "live.text", 0 ],
			"obj-7::obj-102" : [ "live.text[17]", "live.text", 0 ],
			"obj-7::obj-17" : [ "Master Gain[2]", "Master Gain", 0 ],
			"obj-7::obj-98" : [ "live.menu[5]", "live.menu", 0 ],
			"obj-4::obj-45" : [ "live.text[3]", "live.text", 0 ],
			"obj-4::obj-48" : [ "live.menu[2]", "live.menu", 0 ],
			"obj-7::obj-12::obj-13" : [ "Release[1]", "Release", 0 ],
			"obj-4::obj-1::obj-12" : [ "Lookahead", "Lookahead", 0 ],
			"obj-7::obj-100" : [ "live.text[14]", "live.text[1]", 0 ],
			"obj-7::obj-99" : [ "live.menu[6]", "live.menu[2]", 0 ],
			"obj-7::obj-12::obj-30" : [ "live.menu[4]", "live.menu", 0 ],
			"obj-7::obj-108" : [ "live.text[13]", "live.text[4]", 0 ],
			"obj-4::obj-58" : [ "live.text[6]", "live.text[4]", 0 ],
			"obj-4::obj-74" : [ "live.dial[1]", "Transpose", 0 ],
			"obj-7::obj-104" : [ "live.dial[4]", "Depth", 0 ],
			"obj-7::obj-106" : [ "live.dial[3]", "Release", 0 ],
			"obj-7::obj-12::obj-6" : [ "live.text[11]", "live.text", 0 ],
			"obj-5::obj-15" : [ "live.menu[3]", "live.menu", 0 ],
			"obj-7::obj-82" : [ "pan[1]", "Pan", 0 ],
			"obj-7::obj-12::obj-42" : [ "live.text[10]", "live.text", 0 ],
			"obj-4::obj-1::obj-42" : [ "live.text[1]", "live.text", 0 ],
			"obj-4::obj-108" : [ "live.text[7]", "live.text[4]", 0 ],
			"obj-7::obj-12::obj-45" : [ "live.text[12]", "live.text", 0 ],
			"obj-7::obj-90" : [ "live.text[15]", "live.text[1]", 0 ],
			"obj-7::obj-107" : [ "live.dial[5]", "Preamp", 0 ],
			"obj-7::obj-97" : [ "live.numbox[9]", "CPU", 0 ],
			"obj-5::obj-22" : [ "live.text[9]", "live.text", 0 ],
			"obj-4::obj-1::obj-30" : [ "live.menu", "live.menu", 0 ],
			"obj-5::obj-12" : [ "live.numbox[4]", "live.numbox[4]", 0 ],
			"obj-4::obj-1::obj-45" : [ "live.text[2]", "live.text", 0 ],
			"obj-4::obj-1::obj-6" : [ "live.text", "live.text", 0 ],
			"obj-4::obj-4" : [ "live.text[4]", "live.text[1]", 0 ],
			"obj-4::obj-62" : [ "live.text[5]", "live.text[3]", 0 ],
			"obj-7::obj-12::obj-15" : [ "Postamp[1]", "Postamp", 0 ],
			"obj-4::obj-55" : [ "Master Gain[1]", "Master Gain", 0 ]
		}
,
		"dependency_cache" : [ 			{
				"name" : "input~.model.maxpat",
				"bootpath" : "~/Documents/Developpements/Jamoma/Max/Jamoma/patchers/models/audio/stereo/sources/input",
				"type" : "JSON",
				"implicit" : 1
			}
, 			{
				"name" : "limiter~.model.maxpat",
				"bootpath" : "~/Documents/Developpements/Jamoma/Max/Jamoma/patchers/models/audio/stereo/dynamics/limiter",
				"type" : "JSON",
				"implicit" : 1
			}
, 			{
				"name" : "limiter.parametersAndMessages.maxpat",
				"bootpath" : "~/Documents/Developpements/Jamoma/Max/Jamoma/patchers/models/audio/stereo/dynamics/limiter",
				"type" : "JSON",
				"implicit" : 1
			}
, 			{
				"name" : "thru.maxpat",
				"bootpath" : "C74:/patchers/m4l/Pluggo for Live resources/patches",
				"type" : "JSON",
				"implicit" : 1
			}
, 			{
				"name" : "balance~.model.maxpat",
				"bootpath" : "~/Documents/Developpements/Jamoma/Max/Jamoma/patchers/models/audio/stereo/imaging/balance",
				"type" : "JSON",
				"implicit" : 1
			}
, 			{
				"name" : "filter~.model.maxpat",
				"bootpath" : "~/Documents/Developpements/Jamoma/Max/Jamoma/patchers/models/audio/stereo/eq/filter",
				"type" : "JSON",
				"implicit" : 1
			}
, 			{
				"name" : "j.octavebandwidth2q.maxpat",
				"bootpath" : "~/Documents/Developpements/Jamoma/Max/Jamoma/patchers/components/audio/octavebandwidth2q",
				"type" : "JSON",
				"implicit" : 1
			}
, 			{
				"name" : "j.q2octavebandwidth.maxpat",
				"bootpath" : "~/Documents/Developpements/Jamoma/Max/Jamoma/patchers/components/audio/q2octavebandwith",
				"type" : "JSON",
				"implicit" : 1
			}
, 			{
				"name" : "output~.model.maxpat",
				"bootpath" : "~/Documents/Developpements/Jamoma/Max/Jamoma/patchers/models/audio/stereo/output/output",
				"type" : "JSON",
				"implicit" : 1
			}
, 			{
				"name" : "saturation~.model.maxpat",
				"bootpath" : "~/Documents/Developpements/Jamoma/Max/Jamoma/patchers/models/audio/stereo/distortion/saturation",
				"type" : "JSON",
				"implicit" : 1
			}
, 			{
				"name" : "record~.model.maxpat",
				"bootpath" : "~/Documents/Developpements/Jamoma/Max/Jamoma/patchers/models/audio/stereo/output/record",
				"type" : "JSON",
				"implicit" : 1
			}
, 			{
				"name" : "record.parametersAndMessages.maxpat",
				"bootpath" : "~/Documents/Developpements/Jamoma/Max/Jamoma/patchers/models/audio/stereo/output/record",
				"type" : "JSON",
				"implicit" : 1
			}
, 			{
				"name" : "input~.view.maxpat",
				"bootpath" : "~/Documents/Developpements/Jamoma/Max/Jamoma/patchers/models/audio/stereo/sources/input",
				"type" : "JSON",
				"implicit" : 1
			}
, 			{
				"name" : "limiter~.view.maxpat",
				"bootpath" : "~/Documents/Developpements/Jamoma/Max/Jamoma/patchers/models/audio/stereo/dynamics/limiter",
				"type" : "JSON",
				"implicit" : 1
			}
, 			{
				"name" : "filter~.view.maxpat",
				"bootpath" : "~/Documents/Developpements/Jamoma/Max/Jamoma/patchers/models/audio/stereo/eq/filter",
				"type" : "JSON",
				"implicit" : 1
			}
, 			{
				"name" : "output~.view.maxpat",
				"bootpath" : "~/Documents/Developpements/Jamoma/Max/Jamoma/patchers/models/audio/stereo/output/output",
				"type" : "JSON",
				"implicit" : 1
			}
, 			{
				"name" : "j.minuit_device.maxpat",
				"bootpath" : "~/Documents/Developpements/Jamoma/Max/Jamoma/patchers/components/protocol",
				"type" : "JSON",
				"implicit" : 1
			}
, 			{
				"name" : "j.parameter.mxo",
				"type" : "iLaX"
			}
, 			{
				"name" : "j.message.mxo",
				"type" : "iLaX"
			}
, 			{
				"name" : "j.in~.mxo",
				"type" : "iLaX"
			}
, 			{
				"name" : "j.out~.mxo",
				"type" : "iLaX"
			}
, 			{
				"name" : "j.model.mxo",
				"type" : "iLaX"
			}
, 			{
				"name" : "j.limiter~.mxo",
				"type" : "iLaX"
			}
, 			{
				"name" : "j.return.mxo",
				"type" : "iLaX"
			}
, 			{
				"name" : "j.unit.mxo",
				"type" : "iLaX"
			}
, 			{
				"name" : "j.panorama~.mxo",
				"type" : "iLaX"
			}
, 			{
				"name" : "j.stats.mxo",
				"type" : "iLaX"
			}
, 			{
				"name" : "j.overdrive~.mxo",
				"type" : "iLaX"
			}
, 			{
				"name" : "j.ui.mxo",
				"type" : "iLaX"
			}
, 			{
				"name" : "j.remote.mxo",
				"type" : "iLaX"
			}
, 			{
				"name" : "j.view.mxo",
				"type" : "iLaX"
			}
, 			{
				"name" : "j.savebang.mxo",
				"type" : "iLaX"
			}
, 			{
				"name" : "j.receive.mxo",
				"type" : "iLaX"
			}
, 			{
				"name" : "j.receive~.mxo",
				"type" : "iLaX"
			}
, 			{
				"name" : "j.send.mxo",
				"type" : "iLaX"
			}
, 			{
				"name" : "j.modular.mxo",
				"type" : "iLaX"
			}
, 			{
				"name" : "j.pass.mxo",
				"type" : "iLaX"
			}
 ],
		"embedsnapshot" : 0
	}

}
