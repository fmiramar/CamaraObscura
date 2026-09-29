import re
import os

base_dir = "HelpSource/Classes"

# Extracted from earlier work, these are the core parameters for each UGen's main example.
ugens = {
    'DarkVelvetReverb': {
        'model_class': 'DarkVelvetProfile.exponential',
        'model_args': '3, 2, sampleRate: s.sampleRate, seed: 1',
        'ugen_call': 'DarkVelvetReverb.ar(input, ~model, 2, 1600, 1, 19)',
        'creative_name': 'Generating a granular drone using a non-decaying profile',
        'creative_model': '~droneProfile = DarkVelvetProfile.envelope([0, 0.1, 4.9, 5], [-60, 0, 0, -60], sampleRate: s.sampleRate, seed: 123).load(s);',
        'creative_play': 'var trigger = Dust.ar(15);\n    var pitchedPulse = BPF.ar(WhiteNoise.ar, TRand.ar(300, 3000, trigger), 0.1) * trigger;\n    DarkVelvetReverb.ar(pitchedPulse, ~droneProfile, 2, density: 500, spread: 1, seed: 123) * 0.5'
    },
    'GeometryReverb': {
        'model_class': 'GeometryReverbScene.shoebox',
        'model_args': '[5, 5, 5], 0.3, sampleRate: s.sampleRate, seed: 1, grid: 2, lateT60: 1',
        'ugen_call': 'GeometryReverb.ar(input, ~model, [2, 2, 2], [5, 5, 5], [1, 1, 1])[0]',
        'creative_name': 'Moving sound source inside the geometry',
        'creative_model': '~geoModel = GeometryReverbScene.shoebox([10, 8, 4], 0.1, sampleRate: s.sampleRate, grid: 2, lateT60: 2).load(s);',
        'creative_play': 'var sourceX = SinOsc.kr(0.1).range(1, 9);\n    var sourceY = SinOsc.kr(0.15).range(1, 7);\n    GeometryReverb.ar(Dust.ar(5), ~geoModel, [sourceX, sourceY, 2], [10, 8, 4], [2, 2, 2])[0]'
    },
    'GroupedFDN': {
        'model_class': 'GroupedFDNModel.twoRooms',
        'model_args': '0.5, 4, 0.2, s.sampleRate, 2',
        'ugen_call': 'GroupedFDN.ar(input, ~model, 2, 4)[0]',
        'creative_name': 'Extreme energy transfer with LFO modulation',
        'creative_model': '~grpModel = GroupedFDNModel.twoRooms(0.1, 8, 0.05, s.sampleRate, 4).load(s);',
        'creative_play': 'var modInput = BPF.ar(WhiteNoise.ar * Decay.ar(Impulse.ar(0.5), 0.1), SinOsc.kr(0.2).range(400, 2000));\n    GroupedFDN.ar(modInput, ~grpModel, 2, 4)[0]'
    },
    'ModalPlate': {
        'model_class': 'ModalPlateModel.rectangular',
        'model_args': '1.0, 1.5, 0.002, 7850, 2e11, 0.3, 0.01, 512, \\simplySupported, s.sampleRate',
        'ugen_call': 'ModalPlate.ar(input, ~model, [0.3, 0.3], [0.7, 0.7])[0]',
        'creative_name': 'Scanning the plate with a moving pickup',
        'creative_model': '~plateModel = ModalPlateModel.rectangular(2.0, 2.0, 0.005, 7850, 2e11, 0.3, 0.005, 512, \\simplySupported, s.sampleRate).load(s);',
        'creative_play': 'var pickupX = LFDNoise3.kr(0.5).range(0.1, 0.9);\n    var pickupY = LFDNoise3.kr(0.4).range(0.1, 0.9);\n    ModalPlate.ar(Dust.ar(10), ~plateModel, [0.5, 0.5], [pickupX, pickupY])[0]'
    },
    'ModalReverbBank': {
        'model_class': 'ModalReverbModel.harmonic',
        'model_args': '100, 50, 0.1, 2, s.sampleRate',
        'ugen_call': 'ModalReverbBank.ar(input, ~model)[0]',
        'creative_name': 'Pitch-shifting the modal resonators dynamically',
        'creative_model': '~modalModel = ModalReverbModel.harmonic(150, 60, 0.05, 3, s.sampleRate).load(s);',
        'creative_play': 'var pitchMod = SinOsc.kr(0.1).range(0.8, 1.2);\n    ModalReverbBank.ar(Dust.ar(8), ~modalModel, pitchMod)[0]'
    },
    'RIRFDN': {
        'model_class': 'RIRFDNModel.synthetic',
        'model_args': 'sampleRate: s.sampleRate',
        'ugen_call': 'RIRFDN.ar(input, ~model)[0]',
        'creative_name': 'Infinite freeze via feedback modulation',
        'creative_model': '~rirModel = RIRFDNModel.synthetic(sampleRate: s.sampleRate).load(s);',
        'creative_play': 'var freezeEnv = LFPulse.kr(0.2, 0, 0.5).range(1.0, 1.1);\n    RIRFDN.ar(Dust.ar(5), ~rirModel, t60Mult: freezeEnv)[0]'
    },
    'VelvetFDN': {
        'model_class': 'VelvetFDNModel.default',
        'model_args': '8, 3, s.sampleRate',
        'ugen_call': 'VelvetFDN.ar(input, ~model)[0]',
        'creative_name': 'Pumping the dense tail with heavy compression',
        'creative_model': '~velvetModel = VelvetFDNModel.default(12, 4, s.sampleRate).load(s);',
        'creative_play': 'var denseWet = VelvetFDN.ar(Dust.ar(4), ~velvetModel)[0];\n    Compander.ar(denseWet, denseWet, 0.1, 1, 0.1, 0.01, 0.1) * 3'
    }
}

for ugen, props in ugens.items():
    file_path = os.path.join(base_dir, f"{ugen}.schelp")
    if not os.path.exists(file_path):
        continue
    
    with open(file_path, 'r') as f:
        content = f.read()
    
    # Isolate everything before 'examples::'
    parts = content.split("examples::")
    if len(parts) < 2:
        continue
    header = parts[0]
    
    new_examples = f"""examples::

The following examples have been structured into individual blocks so you can easily run them step-by-step.

code::
// Boot the server first!
s.boot;

// 1. Prepare the model and source material
(
~path = "~/Desktop/SuperCollider UGen Ports/04-birosca/CamaraObscura/sounds/".standardizePath;

~source = Buffer.readChannel(s, ~path +/+ "vocal.wav", channels: [0]);
// ~source = Buffer.readChannel(s, ~path +/+ "drum1.wav", channels: [0]);
// ~source = Buffer.readChannel(s, ~path +/+ "drum2.wav", channels: [0]);
// ~source = Buffer.readChannel(s, ~path +/+ "guitar.wav", channels: [0]);
// ~source = Buffer.readChannel(s, ~path +/+ "violin.wav", channels: [0]);

~model = {props['model_class']}({props['model_args']}).load(s);
)

// 2. Play the reverb (wait a moment for the buffers to load before executing this)
(
x = {{
    var input = PlayBuf.ar(1, ~source, BufRateScale.kr(~source), loop: 1);
    // var input = SoundIn.ar(0); // alternative live input
    
    var fade = EnvGen.kr(Env([0, 1], [0.12]));
    var wet = {props['ugen_call']};
    
    (input + wet) * fade * 0.25 ! 2;
}}.play;
)

// 3. Clean up
x.free;
~source.free;
~model.free;
::

// Creative Example: {props['creative_name']}
code::
// 1. Load the model
~creativeModel = {props['creative_model'].replace('~droneProfile', '~creativeModel').replace('~geoModel', '~creativeModel').replace('~grpModel', '~creativeModel').replace('~plateModel', '~creativeModel').replace('~modalModel', '~creativeModel').replace('~rirModel', '~creativeModel').replace('~velvetModel', '~creativeModel')}

// 2. Play
(
y = {{
    {props['creative_play'].replace('~droneProfile', '~creativeModel').replace('~geoModel', '~creativeModel').replace('~grpModel', '~creativeModel').replace('~plateModel', '~creativeModel').replace('~modalModel', '~creativeModel').replace('~rirModel', '~creativeModel').replace('~velvetModel', '~creativeModel')}
}}.play;
)

// 3. Clean up
y.free;
~creativeModel.free;
::
"""
    with open(file_path, 'w') as f:
        f.write(header + new_examples)

print("Rewrote all UGen examples")
