(
var server = Server(
    \creativeReverbsSmoke,
    NetAddr("127.0.0.1", 57145)
);

server.options.numOutputBusChannels = 2;
server.options.numInputBusChannels = 0;
server.options.numAudioBusChannels = 256;
server.options.blockSize = 64;
server.options.sampleRate = 48000;
server.options.memSize = 262144;

server.waitForBoot({
    Routine({
        var models, buffers, recordBuffer;
        var frames = 16384;
        var channels = 15;
        var data, condition, peaks;
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
            recordBuffer = Buffer.alloc(server, frames, channels);
            server.sync;

            SynthDef(\creativeReverbsSmoke, {
                |recordBuf,
                darkBuf, groupedBuf, velvetBuf, velvetMatrixBuf,
                rirBuf, rirMorphBuf, modalBuf, plateBuf, sceneBuf|
                var input = Impulse.ar(11);
                var signals = Array.new;
                signals = signals ++ DarkVelvetReverb.ar(
                    input, darkBuf, 2, 1200, 1, 19
                );
                signals = signals ++ GroupedFDN.ar(
                    input, groupedBuf, 2, 4, 0.65
                );
                signals = signals ++ VelvetFDN.ar(
                    input, velvetBuf, 8, 1, 1
                );
                signals = signals ++ VelvetFDN.ar(
                    input, velvetMatrixBuf, 4, 1, 1
                );
                signals = signals ++ RIRFDN.ar(
                    input, rirBuf, rirMorphBuf,
                    SinOsc.kr(0.7).range(0, 1)
                );
                signals = signals ++ ModalReverbBank.ar(
                    input, modalBuf, 64,
                    pitch: 0.98, decayScale: 0.7
                );
                signals = signals.add(ModalPlate.ar(
                    input, plateBuf,
                    pickupX: SinOsc.kr(1).range(0.2, 0.8)
                ));
                signals = signals ++ GeometryReverb.ar(
                    input, sceneBuf,
                    sourcePosition: [
                        SinOsc.kr(0.7).range(0.5, 4.5), 1, 1
                    ],
                    listenerPosition: [3.5, 3, 1.2]
                );
                RecordBuf.ar(
                    signals, recordBuf,
                    loop: 0, doneAction: Done.freeSelf
                )
            }).add;
            server.sync;
            Synth(\creativeReverbsSmoke, [
                \recordBuf, recordBuffer,
                \darkBuf, buffers[0],
                \groupedBuf, buffers[1],
                \velvetBuf, buffers[2],
                \velvetMatrixBuf, buffers[3],
                \rirBuf, buffers[4],
                \rirMorphBuf, buffers[5],
                \modalBuf, buffers[6],
                \plateBuf, buffers[7],
                \sceneBuf, buffers[8]
            ], server);
            (frames / 48000 + 0.3).wait;
            server.sync;
            condition = Condition({ data.notNil });
            recordBuffer.loadToFloatArray(action: { |array|
                data = array;
                condition.signal
            });
            condition.wait;
            if(data.size != (frames * channels)) {
                Error("CamaraObscura smoke size mismatch.").throw
            };
            if(data.any { |value|
                value.isNaN or: { value.abs >= inf }
            }) {
                Error("CamaraObscura emitted NaN or infinity.").throw
            };
            peaks = Array.fill(channels, { |channel|
                Array.fill(frames, { |frame|
                    data[(frame * channels) + channel].abs
                }).maxItem
            });
            [
                [0, 1], [2, 3], [4, 5], [6, 7],
                [8, 9], [10, 11], [12], [13, 14]
            ].do { |group, index|
                if(group.collect { |channel| peaks[channel] }
                    .maxItem < 1e-7) {
                    Error(
                        "CamaraObscura group % remained silent."
                        .format(index)
                    ).throw
                }
            };
            "CREATIVE_REVERBS_LIVE_SMOKE_OK peaks=%"
            .format(peaks.round(0.0001)).postln;
            recordBuffer.free;
            buffers.do(_.free);
            server.sync;
            server.quit;
            0.exit
        } { |error|
            error.reportError;
            if(recordBuffer.notNil) { recordBuffer.free };
            if(buffers.notNil) { buffers.do(_.free) };
            server.quit;
            1.exit
        }
    }).play(SystemClock)
}, 20, {
    "CamaraObscura smoke server failed to boot.".error;
    1.exit
});

server.boot;
)
