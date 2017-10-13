(

s.options.sampleRate = 48000;
s.boot;
n = 4;

s.waitForBoot({

	~dev = OSSIA_Device("fmcollider").exposeOSCQueryServer(1234, 5678, {

		// array of parameters
		~carfreq = OSSIA_Parameter.array(n, ~dev, "/synth.%/carfreq", Float, [0, 20000], 220, 'clip');
		~modratio = OSSIA_Parameter.array(n, ~dev, "/synth.%/modulation/ratio",
			Float, [0.01, 10], 2, 'clip');
		~modindex = OSSIA_Parameter.array(n, ~dev, "/synth.%/modulation/index", Float,
			[0.01, 1000], 100, 'clip');
		~gain = OSSIA_Parameter.array(n, ~dev, "/synth.%/gain", Float, [-100, 12], 0, 'clip');
		~pan = OSSIA_Parameter.array(n, ~dev, "/synth.%/pan", Float, [-1, 1], 0, 'clip');
		~cutoff = OSSIA_Parameter.array(n, ~dev, "/synth.%/cutoff/frequency",
			Float, [20, 20000], 20000, 'clip');

		~master = OSSIA_Parameter(~dev, '/master/gain', Float, [0, 1], 0.2);

		// start/stop parameters
		~start = OSSIA_Parameter(~dev, 'start', Impulse)
		.callback = {
			var args = Array.fill(n, {|i|
				~carfreq[i].aar ++
				~modratio[i].aar ++
				~modindex[i].aar ++
				~pan[i].aar ++
				~cutoff[i].aar ++
				~gain[i].aar
			});

			args = args.flatten ++ ~master.aar;

			Synth('fmdef', args);

		};

		~stop = OSSIA_Parameter(~dev, 'stop', Impulse)
		.callback = { s.freeAll };

		// synthdef
		~def = SynthDef('fmdef', {
			var fm = Mix.new(
				Array.fill(n, { |i|
					Pan2.ar(
						LPF.ar(
							SinOsc.ar(~carfreq[i].kr +
								SinOsc.ar(~carfreq[i].kr * ~modratio[i].kr, 0, ~modindex[i].kr),
								0, ~gain[i].kr.dbamp),
							~cutoff[i].kr),
						~pan[i].kr);
			}));
				Out.ar(0, fm * ~master.kr);
		}).add;
	});
});


)

