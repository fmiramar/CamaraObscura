(
s.waitForBoot {
    Routine({
        var source = Buffer.readChannel(
            s, ExampleFiles.child, channels: [0]
        );
        var short = RIRFDNModel.synthetic(
            0.8, 0.35, 8, s.sampleRate, 2
        ).load(s);
        var long = RIRFDNModel.synthetic(
            4.5, 2.1, 8, s.sampleRate, 11
        ).load(s);
        s.sync;
        SynthDef(\rirRoomMorph, {
            |sourceBuf, shortBuf, longBuf|
            var input = PlayBuf.ar(
                1, sourceBuf, BufRateScale.kr(sourceBuf), loop: 1
            );
            // var input = SoundIn.ar(0);
            var morph = SinOsc.kr(0.025).range(0, 1);
            Out.ar(0, RIRFDN.ar(
                input, shortBuf, longBuf, morph
            ) * EnvGen.kr(Env([0, 1], [0.08])) * 0.3)
        }).play(args: [
            \sourceBuf, source, \shortBuf, short, \longBuf, long
        ]);
    }).play;
};
)
