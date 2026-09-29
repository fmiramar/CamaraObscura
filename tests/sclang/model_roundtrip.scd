(
var directory = PathName.tmp +/+
    ("creative_reverbs_roundtrip_" ++ UniqueID.next.asString);
var sampleRate = 16000;
var models, readers;
var audioPath, soundFile, signal, extracted;
var writeData;

File.mkdir(directory);
writeData = { |path, data|
    File.use(path, "w", { |file|
        file.write("CREATIVE_REVERBS_MODEL\n");
        data.do { |value|
            file.write(value.asString);
            file.write("\n")
        }
    })
};
models = [
    DarkVelvetProfile.envelope(
        [0, 0.2, 0.6], [0, -12, -60],
        sampleRate: sampleRate, seed: 1
    ),
    GroupedFDNModel.fromGroups(
        [
            [0.3, 0.2, 0.018, 0.04, 2],
            [0.8, 0.5, 0.04, 0.07, 2],
            [1.4, 0.7, 0.07, 0.11, 2]
        ],
        [
            [1, 0.2, 0.5],
            [0.2, 1, 0.8],
            [0.5, 0.8, 1]
        ],
        sampleRate, 2
    ),
    VelvetFDNModel.default(4, 0.8, sampleRate, 3),
    RIRFDNModel.synthetic(0.8, 0.4, 4, sampleRate, 4),
    ModalReverbModel.random(
        16, [80, 7000], [0.2, 0.8], 5, sampleRate
    ),
    ModalPlateModel.rectangular(
        1.2, 0.8, 0.0007, 7850,
        2e11, 0.3, 0.02,
        modes: 16, sampleRate: sampleRate
    ),
    GeometryReverbScene.shoebox(
        [5, 4, 3], 0.3, sampleRate, 6, 2, 0.8
    )
];
readers = [
    DarkVelvetProfile,
    GroupedFDNModel,
    VelvetFDNModel,
    RIRFDNModel,
    ModalReverbModel,
    ModalPlateModel,
    GeometryReverbScene
];
models.do { |model, index|
    var path = directory +/+ ("model_" ++ index ++ ".txt");
    var invalidPath = directory +/+
        ("invalid_" ++ index ++ ".txt");
    var restored, error, invalid, rejected = false;
    model.write(path);
    restored = readers[index].read(path);
    error = (model.asData - restored.asData).abs.maxItem;
    if(error > 1e-5) {
        Error(
            "Prepared-model round trip changed data by %."
            .format(error)
        ).throw
    };
    restored.validate;
    restored.summary;
    invalid = model.asData.copy;
    invalid[12] = if(index < 4) { 2 } { 0 };
    writeData.(invalidPath, invalid);
    try {
        readers[index].read(invalidPath)
    } {
        rejected = true
    };
    if(rejected.not) {
        Error(
            "Family-specific validation accepted corrupt model %."
            .format(index)
        ).throw
    };
    File.delete(path);
    File.delete(invalidPath)
};

audioPath = directory +/+ "modal_fixture.wav";
signal = FloatArray.fill(sampleRate * 2, { |frame|
    var time = frame / sampleRate;
    var direct = if(frame == 0) { 0.8 } { 0 };
    direct
        + (sin(2pi * 311 * time)
            * exp(-6.907755 * time / 0.8) * 0.3)
        + (sin(2pi * 3023 * time)
            * exp(-6.907755 * time / 0.35) * 0.12)
});
soundFile = SoundFile.openWrite(
    audioPath, "WAV", "int16", 1, sampleRate
);
if(soundFile.isNil) {
    Error("Could not create modal extraction fixture.").throw
};
soundFile.writeData(signal);
soundFile.close;
extracted = ModalReverbModel.fromIR(audioPath, 16);
if(extracted.fieldA != 16
    or: { extracted.sampleRate != sampleRate }) {
    Error("ModalReverbModel.fromIR returned the wrong model.").throw
};
File.delete(audioPath);
File.delete(directory);
"CREATIVE_REVERBS_MODEL_ROUNDTRIP_OK".postln;
0.exit
)
