(
s.waitForBoot {
    Routine({
        var source = Buffer.readChannel(
            s, ExampleFiles.child, channels: [0]
        );
        var model = ModalPlateModel.rectangular(
            1.8, 1, 0.0005, 7850,
            2e11, 0.3, 0.012,
            modes: 384, sampleRate: s.sampleRate
        );
        var modelBuffer = model.load(s);
        s.sync;
        SynthDef(\plateMovingPickup, {
            |sourceBuf, modelBuf|
            var input = PlayBuf.ar(
                1, sourceBuf, BufRateScale.kr(sourceBuf), loop: 1
            );
            // var input = SoundIn.ar(0);
            var pickup = SinOsc.kr(0.05).range(0.08, 0.92);
            var wet = ModalPlate.ar(
                input, modelBuf, pickupX: pickup, pickupY: 0.57
            );
            Out.ar(0, Pan2.ar(
                wet * EnvGen.kr(Env([0, 1], [0.06])) * 0.25
            ))
        }).play(args: [
            \sourceBuf, source, \modelBuf, modelBuffer
        ]);
    }).play;
};
)
