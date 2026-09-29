(
s.waitForBoot {
    Routine({
        var source = Buffer.readChannel(
            s, ExampleFiles.child, channels: [0]
        );
        var model = ModalReverbModel.harmonic(
            55, 256, 5, s.sampleRate
        );
        var modelBuffer = model.load(s);
        s.sync;
        SynthDef(\modalHarmonicRoom, {
            |sourceBuf, modelBuf|
            var input = PlayBuf.ar(
                1, sourceBuf, BufRateScale.kr(sourceBuf), loop: 1
            );
            // var input = SoundIn.ar(0);
            var pitch = LFTri.kr(0.035).exprange(0.5, 2);
            Out.ar(0, ModalReverbBank.ar(
                input, modelBuf, 256,
                pitch: pitch, dispersion: 0.1, drive: 0.5
            ) * EnvGen.kr(Env([0, 1], [0.06])) * 0.3)
        }).play(args: [
            \sourceBuf, source, \modelBuf, modelBuffer
        ]);
    }).play;
};
)
