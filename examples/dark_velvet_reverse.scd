(
s.waitForBoot {
    Routine({
        var source = Buffer.readChannel(
            s, ExampleFiles.child, channels: [0]
        );
        var profile = DarkVelvetProfile.envelope(
            [0, 0.8, 2.2, 3], [-70, -30, -5, 0],
            sampleRate: s.sampleRate, seed: 21
        );
        var profileBuffer = profile.load(s);
        s.sync;
        SynthDef(\darkVelvetReverse, {
            |sourceBuf, profileBuf|
            var input = PlayBuf.ar(
                1, sourceBuf, BufRateScale.kr(sourceBuf), loop: 1
            );
            // var input = SoundIn.ar(0);
            Out.ar(0, DarkVelvetReverb.ar(
                input, profileBuf, 2, 1300, 0.9, 21
            ) * EnvGen.kr(Env([0, 1], [0.12])) * 0.25)
        }).play(args: [
            \sourceBuf, source, \profileBuf, profileBuffer
        ]);
    }).play;
};
)
