(
// --------------------------------------------------------------------------------------------------------
s.options.sampleRate = 44100;
s.boot;
n = 4;
// --------------------------------------------------------------------------------------------------------
s.waitForBoot({

	b = Buffer.alloc(s, 512, 1, { |buf|
		buf.chebyMsg([0.25, 0.5, 0.25]);
	});

// --------------------------------------------------------------------------------------------------------
	~device = OSSIA_Device("fmsynth").exposeOSCQueryServer(1234, 5678, {
// --------------------------------------------------------------------------------------------------------
		// define array of parameters in device tree-structure
		~carfreq = OSSIA_Parameter.array(n, ~device, "/synth.%/carrier/frequency", Float, [0, 20000], 220, 'clip');
		~modratio = OSSIA_Parameter.array(n, ~device, "/synth.%/modulation/ratio",
			Float, [0.01, 10], 2, 'clip');
		~modindex = OSSIA_Parameter.array(n, ~device, "/synth.%/modulation/index", Float,
			[0.01, 1000], 100, 'clip');
		~gain = OSSIA_Parameter.array(n, ~device, "/synth.%/gain", Float, [-100, 12], 0, 'clip');
		~pan = OSSIA_Parameter.array(n, ~device, "/synth.%/pan", Float, [-1, 1], 0, 'clip');

		~master = OSSIA_Parameter(~device, '/master/gain', Float, [-100, 12], -6);
// --------------------------------------------------------------------------------------------------------
		// start/stop parameters
		~start = OSSIA_Parameter(~device, 'start', Impulse)
		// make a snapshot of ~device excluding ~start and ~stop parameters
		.callback = { Synth('fmdef', ~device.snapshot(~start, ~stop)) };

		~stop = OSSIA_Parameter(~device, 'stop', Impulse)
		.callback = { s.freeAll };
// --------------------------------------------------------------------------------------------------------
		~def = SynthDef('fmdef', {
			var fm = Mix.new(
				Array.fill(n, { |i|
					Pan2.ar(
							SinOsc.ar(~carfreq[i].kr +
								SinOsc.ar(~carfreq[i].kr * ~modratio[i].kr, 0, ~modindex[i].kr),
								0, ~gain[i].kr.dbamp),
						~pan[i].kr);
			}));

			var dist = Mix.ar([Shaper.ar(b, HPF.ar(fm, 1000), 0.3), fm]);
			var comp = Compander.ar(dist, dist, -30.dbamp, 1, 1/3, mul: 18.dbamp);
			var shelf = BHiShelf.ar(comp, 5000, 1, 6);
			var master = shelf * ~master.kr.dbamp;
			Out.ar(0, FreeVerb2.ar(master[0], master[1], 0.3));
		}).add;
	});
});
// --------------------------------------------------------------------------------------------------------
)

~carfreq[0].v = 60.midicps;