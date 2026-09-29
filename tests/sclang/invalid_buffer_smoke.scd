(
var server = Server(
    \creativeReverbsInvalid,
    NetAddr("127.0.0.1", 57147)
);

server.options.numOutputBusChannels = 2;
server.options.numInputBusChannels = 0;
server.options.memSize = 65536;
server.waitForBoot({
    Routine({
        var invalid = Buffer.alloc(server, 32, 1);
        var definitions, synths;
        server.sync;
        definitions = [
            SynthDef(\invalidDark, { |buf|
                Out.ar(0, DarkVelvetReverb.ar(
                    Impulse.ar(1), buf, 2
                ))
            }),
            SynthDef(\invalidGrouped, { |buf|
                Out.ar(0, GroupedFDN.ar(
                    Impulse.ar(1), buf, 2, 2
                ))
            }),
            SynthDef(\invalidVelvet, { |buf|
                Out.ar(0, VelvetFDN.ar(
                    Impulse.ar(1), buf, 4
                ))
            }),
            SynthDef(\invalidRIR, { |buf|
                Out.ar(0, RIRFDN.ar(
                    Impulse.ar(1), buf
                ))
            }),
            SynthDef(\invalidModal, { |buf|
                Out.ar(0, ModalReverbBank.ar(
                    Impulse.ar(1), buf, 16
                ))
            }),
            SynthDef(\invalidPlate, { |buf|
                Out.ar(0, ModalPlate.ar(
                    Impulse.ar(1), buf
                ) ! 2)
            }),
            SynthDef(\invalidGeometry, { |buf|
                Out.ar(0, GeometryReverb.ar(
                    Impulse.ar(1), buf
                ))
            })
        ];
        definitions.do(_.add);
        server.sync;
        synths = [
            \invalidDark, \invalidGrouped, \invalidVelvet,
            \invalidRIR, \invalidModal, \invalidPlate,
            \invalidGeometry
        ].collect { |name|
            Synth(name, [\buf, invalid], server)
        };
        0.2.wait;
        synths.do(_.free);
        server.sync;
        invalid.free;
        server.sync;
        "CREATIVE_REVERBS_INVALID_BUFFER_OK".postln;
        server.quit;
        0.exit
    }).play(SystemClock)
}, 20, {
    "CamaraObscura invalid-buffer server failed to boot.".error;
    1.exit
});
server.boot;
)
