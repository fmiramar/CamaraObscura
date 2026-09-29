(
var server = Server(
    \creativeReverbsProbe,
    NetAddr("127.0.0.1", 57146)
);

server.options.numOutputBusChannels = 2;
server.options.numInputBusChannels = 0;
server.options.numAudioBusChannels = 256;
server.options.blockSize = 64;
server.options.sampleRate = 48000;
server.options.memSize = 262144;

server.waitForBoot({
    Routine({
        var models, buffers;
        var runProbe;
        try {
            models = [
                DarkVelvetProfile.exponential(
                    0.4, 0.3, sampleRate: 48000, seed: 1
                ),
                GroupedFDNModel.twoRooms(
                    0.3, 1.2, 0.4, 48000, 2
                ),
                VelvetFDNModel.default(8, 0.8, 48000, 3),
                VelvetFDNModel.filteredMatrix(
                    [0.037, 0.043, 0.051, 0.059],
                    0 ! 8, [0.8, 0.45],
                    \paraunitary, 48000, 4
                ),
                RIRFDNModel.synthetic(
                    1.1, 0.6, 8, 48000, 5
                ),
                RIRFDNModel.synthetic(
                    1.8, 0.9, 8, 48000, 6
                ),
                ModalReverbModel.random(
                    64, [90, 10000], [0.2, 1.2], 7, 48000
                ),
                ModalPlateModel.rectangular(
                    1.2, 0.8, 0.0007, 7850,
                    2e11, 0.3, 0.02,
                    modes: 64, sampleRate: 48000
                ),
                GeometryReverbScene.shoebox(
                    [5, 4, 3], 0.3, 48000, 8, 2, 1.0
                )
            ];
            buffers = models.collect { |model| model.load(server) };
            server.sync;

            SynthDef(\creativeReverbsProbeDark, {
                |firstBuf|
                var input = Impulse.ar(11);
                Out.ar(0, DarkVelvetReverb.ar(
                    input, firstBuf, 2, 1200, 1, 19
                ) * 0)
            }).add;
            SynthDef(\creativeReverbsProbeGrouped, {
                |firstBuf|
                Out.ar(0, GroupedFDN.ar(
                    Impulse.ar(11), firstBuf, 2, 4, 0.65
                ) * 0)
            }).add;
            SynthDef(\creativeReverbsProbeVelvet, {
                |firstBuf|
                Out.ar(0, VelvetFDN.ar(
                    Impulse.ar(11), firstBuf,
                    8, 1, 1
                ) * 0)
            }).add;
            SynthDef(\creativeReverbsProbeVelvetMatrix, {
                |firstBuf|
                Out.ar(0, VelvetFDN.ar(
                    Impulse.ar(11), firstBuf,
                    4, 1, 1
                ) * 0)
            }).add;
            SynthDef(\creativeReverbsProbeRIR, {
                |firstBuf, secondBuf = -1|
                Out.ar(0, RIRFDN.ar(
                    Impulse.ar(11), firstBuf, secondBuf, 0.5
                ) * 0)
            }).add;
            SynthDef(\creativeReverbsProbeModal, {
                |firstBuf|
                Out.ar(0, ModalReverbBank.ar(
                    Impulse.ar(11), firstBuf, 64
                ) * 0)
            }).add;
            SynthDef(\creativeReverbsProbePlate, {
                |firstBuf|
                Out.ar(0, ModalPlate.ar(
                    Impulse.ar(11), firstBuf
                ) ! 2 * 0)
            }).add;
            SynthDef(\creativeReverbsProbeGeometry, {
                |firstBuf|
                Out.ar(0, GeometryReverb.ar(
                    Impulse.ar(11), firstBuf,
                    [1, 1, 1], [3.5, 3, 1.2]
                ) * 0)
            }).add;
            server.sync;

            runProbe = {
                |label, definition, first, second = -1|
                var synth;
                "CREATIVE_REVERBS_PROBE_BEGIN %"
                .format(label).postln;
                synth = Synth(definition, [
                    \firstBuf, buffers[first],
                    \secondBuf,
                    if(second >= 0) { buffers[second] } { -1 }
                ], server);
                0.25.wait;
                synth.free;
                server.sync;
                "CREATIVE_REVERBS_PROBE_OK %"
                .format(label).postln
            };

            runProbe.(
                \darkVelvet, \creativeReverbsProbeDark, 0
            );
            runProbe.(
                \groupedFDN, \creativeReverbsProbeGrouped, 1
            );
            runProbe.(
                \velvetFDN, \creativeReverbsProbeVelvet, 2
            );
            runProbe.(
                \velvetMatrix,
                \creativeReverbsProbeVelvetMatrix, 3
            );
            runProbe.(
                \rirFDN, \creativeReverbsProbeRIR, 4, 5
            );
            runProbe.(
                \modalBank, \creativeReverbsProbeModal, 6
            );
            runProbe.(
                \modalPlate, \creativeReverbsProbePlate, 7
            );
            runProbe.(
                \geometry, \creativeReverbsProbeGeometry, 8
            );

            buffers.do(_.free);
            server.sync;
            "CREATIVE_REVERBS_FAMILY_PROBE_OK".postln;
            server.quit;
            0.exit
        } { |error|
            error.reportError;
            if(buffers.notNil) { buffers.do(_.free) };
            server.quit;
            1.exit
        }
    }).play(SystemClock)
}, 20, {
    "CamaraObscura probe server failed to boot.".error;
    1.exit
});

server.boot;
)
