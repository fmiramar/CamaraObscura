(
s.waitForBoot {
    Routine({
        var source = Buffer.readChannel(
            s, ExampleFiles.child, channels: [0]
        );
        var model = VelvetFDNModel.filteredMatrix(
            [0.037, 0.043, 0.051, 0.059],
            0 ! 12, [3, 1.5], \paraunitary,
            s.sampleRate, 9
        );
        var modelBuffer = model.load(s);
        s.sync;
        SynthDef(\velvetDenseTail, {
            |sourceBuf, modelBuf|
            var input = PlayBuf.ar(
                1, sourceBuf, BufRateScale.kr(sourceBuf), loop: 1
            );
            // var input = SoundIn.ar(0);
            var density = LFTri.kr(0.06).range(0.15, 1);
            Out.ar(0, VelvetFDN.ar(
                input, modelBuf, 4, density, 1
            ) * EnvGen.kr(Env([0, 1], [0.08])) * 0.3)
        }).play(args: [
            \sourceBuf, source, \modelBuf, modelBuffer
        ]);
    }).play;
};
)
