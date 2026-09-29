(
var dark, grouped, velvet, velvetMatrix, rir;
var modal, plate, scene;
var krErrors = 0;
var bytes;
var groupedPairCount, firstGroupDelays, secondGroupDelays;
var plateRigidity, plateExpected;

dark = DarkVelvetProfile.exponential(
    1, 0.7, sampleRate: 48000, seed: 2
);
grouped = GroupedFDNModel.twoRooms(
    0.5, 2.0, 0.3, 48000, 3
);
groupedPairCount = 1;
firstGroupDelays = Array.fill(4, { |line|
    grouped.payload[groupedPairCount + (line * 8)]
});
secondGroupDelays = Array.fill(4, { |line|
    grouped.payload[
        groupedPairCount + ((line + 4) * 8)
    ]
});
if(firstGroupDelays.maxItem > (0.053 * 48000)
    or: { secondGroupDelays.minItem < (0.047 * 48000) }) {
    Error(
        "GroupedFDNModel did not preserve per-group delay ranges."
    ).throw
};
velvet = VelvetFDNModel.default(8, 1.5, 48000, 4);
velvetMatrix = VelvetFDNModel.filteredMatrix(
    [0.041, 0.047, 0.053, 0.061],
    0 ! 8, [[2, 1.2]], \paraunitary, 48000, 5
);
rir = RIRFDNModel.synthetic(2.2, 1.2, 8, 48000, 6);
modal = ModalReverbModel.random(
    32, [80, 10000], [0.3, 2], 7, 48000
);
plate = ModalPlateModel.rectangular(
    2.4, 1.2, 0.0005, 7850, 2e11, 0.3, 0.01,
    modes: 32, sampleRate: 48000
);
plateRigidity = (2e11 * 0.0005.cubed)
    / (12 * (1 - 0.3.squared));
plateExpected = (pi / 2)
    * (plateRigidity / (7850 * 0.0005)).sqrt
    * ((1 / 2.4.squared) + (1 / 1.2.squared));
if((plate.payload[2] - plateExpected).abs
    > (plateExpected * 1e-6)) {
    Error(
        "ModalPlateModel frequency does not match Kirchhoff-Love reference."
    ).throw
};
scene = GeometryReverbScene.shoebox(
    [6, 5, 3], 0.25, 48000, 8, 2, 1.5
);

[dark, grouped, velvet, velvetMatrix, rir, modal, plate, scene]
.do { |model|
    if(model.validate != true) {
        Error("model validation did not return true").throw
    };
    if(model.asData.size != (model.payload.size + 12)) {
        Error("model serialization size mismatch").throw
    }
};

bytes = SynthDef(\creativeReverbsCompile, {
    var signal = Impulse.ar(1);
    var output;
    output = DarkVelvetReverb.ar(signal, 0, 2);
    if(output.size != 2) { Error("DarkVelvet outputs").throw };
    Out.ar(0, output * 0);
    output = GroupedFDN.ar(signal, 0, 2, 4);
    if(output.size != 2) { Error("GroupedFDN outputs").throw };
    Out.ar(0, output * 0);
    output = VelvetFDN.ar(signal, 0, 8);
    if(output.size != 2) { Error("VelvetFDN outputs").throw };
    Out.ar(0, output * 0);
    output = RIRFDN.ar(signal, 0);
    if(output.size != 2) { Error("RIRFDN outputs").throw };
    Out.ar(0, output * 0);
    output = ModalReverbBank.ar(signal, 0, 32);
    if(output.size != 2) { Error("ModalReverb outputs").throw };
    Out.ar(0, output * 0);
    Out.ar(0, ModalPlate.ar(signal, 0) * 0);
    output = GeometryReverb.ar(signal, 0);
    if(output.size != 2) { Error("Geometry outputs").throw };
    Out.ar(0, output * 0)
}).asBytes;

[
    DarkVelvetReverb, GroupedFDN, VelvetFDN, RIRFDN,
    ModalReverbBank, ModalPlate, GeometryReverb
].do { |class|
    try { class.kr } { krErrors = krErrors + 1 }
};

if(krErrors != 7) {
    Error("Every CamaraObscura .kr method must reject construction.").throw
};
if(bytes.size < 1) {
    Error("CamaraObscura SynthDef did not compile.").throw
};

"CREATIVE_REVERBS_LANGUAGE_COMPILE_OK".postln;
0.exit;
)
