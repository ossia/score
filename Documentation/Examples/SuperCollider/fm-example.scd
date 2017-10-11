(

s.options.sampleRate = 48000;
s.boot;
n = 4;

s.waitForBoot({

	~dev = OSSIA_Device("ensatt-collider").exposeOSCQueryServer(1234, 5678, {

		~car_freq = Array.fill(n, { |i|
			OSSIA_Parameter(~dev, '/fm_' ++ i ++ '/carfreq', Float, [0, 20000], 220, 'clip');
		});

		~harm_ratio = Array.fill(n, { |i|
			OSSIA_Parameter(~dev, '/fm_' ++ i ++ '/harm_ratio', Float, [0.0001, 10], 2, 'clip');
		});

		~fm_index = Array.fill(n, { |i|
			OSSIA_Parameter(~dev, '/fm_' ++ i ++ '/pm_index', Float, [0.001, 1000], 100, 'clip');
		});

		~fm_level = Array.fill(n, {|i|
			OSSIA_Parameter(~dev, '/fm_' ++ i ++ '/level', Float, [0, 1], 1, 'clip');
		});

		~fm_pan = Array.fill(n, {|i|
			OSSIA_Parameter(~dev, '/fm_' ++ i ++ '/pan', Float, [-1, 1], 1.0.rand2, 'clip');
		});

		~fm_cutoff = Array.fill(n, {|i|
			OSSIA_Parameter(~dev, '/fm_' ++ i ++ '/filter/cutoff', Float, [20, 20000], 20000, 'clip');
		});

		~start = OSSIA_Parameter(~dev, 'start', Impulse);
		~stop = OSSIA_Parameter(~dev, 'stop', Impulse);
		~master = OSSIA_Parameter(~dev, '/master/level', Float, [0, 1], 0.2);

		~args = Array.fill(n, {|i|
			~car_freq[i].aar ++
			~harm_ratio[i].aar ++
			~fm_index[i].aar ++
			~fm_pan[i].aar ++
			~fm_cutoff[i].aar ++
			~fm_level[i].aar
		});

		~args = ~args.flatten ++ ~master.aar;
		~args.postln;

		~start.callback = { Synth('fmdef', ~args) };
		~stop.callback = { s.freeAll };

		~def = SynthDef('fmdef', {
			var fm = Mix.new(
				Array.fill(n, { |i|
					Pan2.ar(
						LPF.ar(
							SinOsc.ar(~car_freq[i].kr +
								SinOsc.ar(~car_freq[i].kr * ~harm_ratio[i].kr, 0, ~fm_index[i].kr),
								0, ~fm_level[i].kr),
							~fm_cutoff[i].kr),
						~fm_pan[i].kr);
			}));
				Out.ar(0, fm * ~master.kr);
		}).add;
	});
});


)
