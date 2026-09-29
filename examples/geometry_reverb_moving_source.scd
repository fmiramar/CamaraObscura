(
s.waitForBoot {
    Routine({
        var source = Buffer.readChannel(
            s, ExampleFiles.child, channels: [0]
        );
        var scene = GeometryReverbScene.shoebox(
            [7, 5, 3], 0.3, s.sampleRate, 4, 3, 2.2
        );
        var sceneBuffer = scene.load(s);
        s.sync;
        SynthDef(\geometryMovingSource, {
            |sourceBuf, sceneBuf|
            var input = PlayBuf.ar(
                1, sourceBuf, BufRateScale.kr(sourceBuf), loop: 1
            );
            // var input = SoundIn.ar(0);
            var x = SinOsc.kr(0.04).range(0.5, 6.5);
            var yaw = LFTri.kr(0.03).range(-pi, pi);
            Out.ar(0, GeometryReverb.ar(
                input, sceneBuf,
                [x, 1.2, 1.3], [4.5, 3.7, 1.4],
                [yaw, 0, 0], [0, 0, 0],
                0.8, 0.7
            ) * EnvGen.kr(Env([0, 1], [0.1])) * 0.25)
        }).play(args: [
            \sourceBuf, source, \sceneBuf, sceneBuffer
        ]);
    }).play;
};
)
