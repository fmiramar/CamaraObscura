import glob

examples = {
    "DarkVelvetReverb": """
// Creative Example: Generating a granular drone using a non-decaying profile
code::
(
s.waitForBoot {
    Routine({
        var profile = DarkVelvetProfile.envelope(
            [0, 0.1, 4.9, 5], 
            [-60, 0, 0, -60], 
            sampleRate: s.sampleRate, 
            seed: 123
        );
        var buffer = profile.load(s);
        s.sync;
        {
            var trigger = Dust.ar(15);
            var pitchedPulse = BPF.ar(WhiteNoise.ar, TRand.ar(300, 3000, trigger), 0.1) * trigger;
            DarkVelvetReverb.ar(
                pitchedPulse, buffer, 2, 
                density: 500, spread: 1, seed: 123
            ) * 0.5
        }.play;
    }).play;
};
)
::
""",
    "GroupedFDN": """
// Creative Example: Dynamic coupling manipulation and freezing
code::
(
s.waitForBoot {
    Routine({
        var model = GroupedFDNModel.synthetic(groups: 3, delaysPerGroup: 4);
        var buffer = model.load(s);
        s.sync;
        {
            var source = Impulse.ar(1) * 0.5;
            // Modulating the coupling amount dynamically sloshes energy between groups
            var coupleMod = SinOsc.kr(0.1).range(0.0, 1.0);
            // Intermittently freeze the reverb tail
            var freezeState = LFPulse.kr(0.2, 0, 0.3);
            GroupedFDN.ar(
                source, buffer, groups: 3, delaysPerGroup: 4, 
                coupling: coupleMod, freeze: freezeState
            )
        }.play;
    }).play;
};
)
::
""",
    "VelvetFDN": """
// Creative Example: Extreme density build-up with rhythmic input
code::
(
s.waitForBoot {
    Routine({
        var model = VelvetFDNModel.synthetic(delayCount: 8);
        var buffer = model.load(s);
        s.sync;
        {
            // A sharp rhythmic sequence
            var env = EnvGen.ar(Env.perc(0.01, 0.1), Impulse.ar(4));
            var source = SinOsc.ar(TExpRand.ar(100, 2000, Impulse.ar(4))) * env * 0.5;
            // Modulate diffusion to create an evolving texture
            var diffMod = LFNoise2.kr(0.5).range(0.2, 1.0);
            VelvetFDN.ar(
                source, buffer, delayCount: 8, 
                density: 8, diffusion: diffMod
            )
        }.play;
    }).play;
};
)
::
""",
    "RIRFDN": """
// Creative Example: Spectral morphing and extreme decay scaling
code::
(
s.waitForBoot {
    Routine({
        var model = RIRFDNModel.synthetic();
        var buffer = model.load(s);
        s.sync;
        {
            var source = Dust.ar(3) * 0.5;
            // Dramatically stretch the decay tail while shifting the tone
            var decayStretch = SinOsc.kr(0.05).range(1.0, 5.0);
            var toneShift = SinOsc.kr(0.07).range(-0.5, 0.5);
            RIRFDN.ar(
                source, buffer, morphBuf: -1, morph: 0,
                decayScale: decayStretch, tone: toneShift
            )
        }.play;
    }).play;
};
)
::
""",
    "ModalReverbBank": """
// Creative Example: Metallic plunge using pitch and dispersion modulation
code::
(
s.waitForBoot {
    Routine({
        var model = ModalReverbModel.synthetic(modeCount: 1024);
        var buffer = model.load(s);
        s.sync;
        {
            var trigger = Impulse.ar(0.5);
            var source = PinkNoise.ar * EnvGen.ar(Env.perc(0.001, 0.05), trigger);
            // Plunge the pitch of the modal resonances after each hit
            var pitchMod = EnvGen.kr(Env([1.5, 0.2], [1.5], \exp), trigger);
            ModalReverbBank.ar(
                source, buffer, modeCount: 1024,
                pitch: pitchMod, decayScale: 2.0
            ) * 0.5
        }.play;
    }).play;
};
)
::
""",
    "ModalPlate": """
// Creative Example: Scanning the physical plate with moving pickups
code::
(
s.waitForBoot {
    Routine({
        var model = ModalPlateModel.synthetic();
        var buffer = model.load(s);
        s.sync;
        {
            var source = LFNoise0.ar(10) * 0.1;
            // Continuously drag the excitation and pickup points across the plate surface
            var exX = LFTri.kr(0.1).range(0.1, 0.9);
            var exY = LFTri.kr(0.15).range(0.1, 0.9);
            var pickX = LFTri.kr(0.07).range(0.1, 0.9);
            var pickY = LFTri.kr(0.12).range(0.1, 0.9);
            
            ModalPlate.ar(
                source, buffer, 
                excitationX: exX, excitationY: exY,
                pickupX: pickX, pickupY: pickY
            )
        }.play;
    }).play;
};
)
::
""",
    "GeometryReverb": """
// Creative Example: A physically impossible breathing room
code::
(
s.waitForBoot {
    Routine({
        var scene = GeometryReverbScene.shoebox(
            [5, 5, 5], 0.5, s.sampleRate, 1, 1, 1
        );
        var buffer = scene.load(s);
        s.sync;
        {
            var source = Impulse.ar(2) * 0.5;
            // The room dimensions oscillate at audio rate, causing intense Doppler effects
            var breathingX = SinOsc.kr(0.2).range(3, 10);
            var breathingY = SinOsc.kr(0.3).range(3, 10);
            var breathingZ = SinOsc.kr(0.25).range(3, 10);
            
            GeometryReverb.ar(
                source, buffer, 
                sourcePosition: [2, 2, 2],
                roomSizes: [breathingX, breathingY, breathingZ],
                listenerPosition: [1, 1, 1]
            ) * 0.5
        }.play;
    }).play;
};
)
::
"""
}

for f in glob.glob("HelpSource/Classes/*.schelp"):
    name = f.split('/')[-1].replace('.schelp', '')
    if name in examples:
        with open(f, 'a') as file:
            file.write(examples[name])
        print(f"Appended example to {name}")

