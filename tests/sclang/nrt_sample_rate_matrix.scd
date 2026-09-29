(
var sampleRates = [44100, 48000, 96000];
var channels = 13;
var duration = 0.8;
var definition = SynthDef(\creativeReverbsNRTMatrix, {
    |darkBuf = 0, groupedBuf = 1, velvetBuf = 2,
    rirBuf = 3, rirMorphBuf = 4, modalBuf = 5,
    plateBuf = 6, sceneBuf = 7|
    var input = Impulse.ar(5);
    var signals = Array.new;
    signals = signals ++ DarkVelvetReverb.ar(
        input, darkBuf, 2, 1000, 1, 13
    );
    signals = signals ++ GroupedFDN.ar(
        input, groupedBuf, 2, 2, 0.7
    );
    signals = signals ++ VelvetFDN.ar(
        input, velvetBuf, 4, 1, 1
    );
    signals = signals ++ RIRFDN.ar(
        input, rirBuf, rirMorphBuf,
        SinOsc.kr(0.8).range(0, 1)
    );
    signals = signals ++ ModalReverbBank.ar(
        input, modalBuf, 32
    );
    signals = signals.add(ModalPlate.ar(
        input, plateBuf, pickupX: 0.63
    ));
    signals = signals ++ GeometryReverb.ar(
        input, sceneBuf,
        [1.2, 1, 1], [3.4, 2.8, 1.2]
    );
    Out.ar(0, signals)
});

Routine({
    try {
        sampleRates.do { |sampleRate|
            var path = Platform.defaultTempDir +/+
                "creative-reverbs-nrt-%.wav".format(sampleRate);
            var models = [
                DarkVelvetProfile.exponential(
                    0.3, 0.18,
                    sampleRate: sampleRate, seed: 1
                ),
                GroupedFDNModel.fromGroups(
                    [
                        [0.18, 0.12, 0.018, 0.05, 2],
                        [0.55, 0.32, 0.05, 0.1, 2]
                    ],
                    [[1, 0.5], [0.5, 1]],
                    sampleRate, 2
                ),
                VelvetFDNModel.default(
                    4, 0.45, sampleRate, 3
                ),
                RIRFDNModel.synthetic(
                    0.5, 0.25, 4, sampleRate, 4
                ),
                RIRFDNModel.synthetic(
                    0.8, 0.4, 4, sampleRate, 5
                ),
                ModalReverbModel.random(
                    32, [80, sampleRate * 0.4],
                    [0.2, 0.6], 6, sampleRate
                ),
                ModalPlateModel.rectangular(
                    1.2, 0.8, 0.0007, 7850,
                    2e11, 0.3, 0.03,
                    modes: 32, sampleRate: sampleRate
                ),
                GeometryReverbScene.shoebox(
                    [5, 4, 3], 0.35,
                    sampleRate, 7, 2, 0.6
                )
            ];
            var messages = List.new;
            var options = ServerOptions.new;
            var completed = FlowVar.new;
            var file, data, frames, peaks;

            models.do { |model, index|
                var values = model.asData;
                messages.add([
                    0, [\b_alloc, index, values.size, 1]
                ]);
                messages.add([
                    0.001,
                    [\b_setn, index, 0, values.size] ++ values
                ])
            };
            messages.add([
                0.01, [\d_recv, definition.asBytes]
            ]);
            messages.add([
                0.02,
                [\s_new, \creativeReverbsNRTMatrix, 1000, 0, 0]
            ]);
            messages.add([
                duration - 0.02, [\n_free, 1000]
            ]);

            options.numOutputBusChannels = channels;
            options.numInputBusChannels = 0;
            options.numAudioBusChannels = 256;
            options.blockSize = 64;
            options.memSize = 262144;
            Score(messages.asArray).recordNRT(
                outputFilePath: path,
                sampleRate: sampleRate,
                headerFormat: "wav",
                sampleFormat: "float",
                options: options,
                duration: duration,
                action: { completed.value = true }
            );
            completed.value;
            file = SoundFile.openRead(path);
            frames = file.numFrames;
            data = FloatArray.newClear(frames * channels);
            file.readData(data);
            file.close;
            File.delete(path);
            if(data.any { |value|
                value.isNaN or: { value.abs >= inf }
            }) {
                Error(
                    "Non-finite NRT output at % Hz."
                    .format(sampleRate)
                ).throw
            };
            peaks = Array.fill(channels, { |channel|
                Array.fill(frames, { |frame|
                    data[(frame * channels) + channel].abs
                }).maxItem
            });
            [
                [0, 1], [2, 3], [4, 5], [6, 7],
                [8, 9], [10], [11, 12]
            ].do { |group, index|
                if(group.collect { |channel| peaks[channel] }
                    .maxItem < 1e-8) {
                    Error(
                        "NRT group % silent at % Hz."
                        .format(index, sampleRate)
                    ).throw
                }
            };
            "CREATIVE_REVERBS_NRT_OK rate=% peaks=%"
            .format(sampleRate, peaks.round(0.0001)).postln
        };
        "CREATIVE_REVERBS_NRT_MATRIX_OK".postln;
        0.exit
    } { |error|
        error.reportError;
        1.exit
    }
}).play(SystemClock);
)
