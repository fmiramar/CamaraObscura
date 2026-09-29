(
s.waitForBoot {
    Routine({
        var source = Buffer.readChannel(
            s, ExampleFiles.child, channels: [0]
        );
        var profile = DarkVelvetProfile.envelope(
            [0, 0.12, 0.9, 1.0], [0, -4, -8, -90],
            sampleRate: s.sampleRate, seed: 12
        );
        var profileBuffer = profile.load(s);
        s.sync;
        SynthDef(\darkVelvetGated, {
            |sourceBuf, profileBuf|
            var input = PlayBuf.ar(
                1, sourceBuf, BufRateScale.kr(sourceBuf), loop: 1
            );
            // var input = SoundIn.ar(0);
            var wet = DarkVelvetReverb.ar(
                input, profileBuf, 2, 1700, 1, 12
            );
            Out.ar(0, wet * EnvGen.kr(Env([0, 1], [0.12])) * 0.25)
        }).play(args: [
            \sourceBuf, source, \profileBuf, profileBuffer
        ]);
    }).play;
};
)
