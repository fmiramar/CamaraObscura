(
s.waitForBoot {
    Routine({
        var source = Buffer.readChannel(
            s, ExampleFiles.child, channels: [0]
        );
        var model = GroupedFDNModel.twoRooms(
            0.4, 4.8, 0.65, s.sampleRate, 7
        );
        var modelBuffer = model.load(s);
        s.sync;
        SynthDef(\groupedRooms, {
            |sourceBuf, modelBuf|
            var input = PlayBuf.ar(
                1, sourceBuf, BufRateScale.kr(sourceBuf), loop: 1
            );
            // var input = SoundIn.ar(0);
            var aperture = SinOsc.kr(0.025).range(0.02, 1);
            Out.ar(0, GroupedFDN.ar(
                input, modelBuf, 2, 4, aperture
            ) * EnvGen.kr(Env([0, 1], [0.08])) * 0.3)
        }).play(args: [
            \sourceBuf, source, \modelBuf, modelBuffer
        ]);
    }).play;
};
)
