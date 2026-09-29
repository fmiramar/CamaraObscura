(
s.waitForBoot {
    Routine({
        var dur = 0.5;
        var frames = (dur * s.sampleRate).asInteger;
        var irDir = "~/Desktop/SuperCollider UGen Ports/04-birosca/CamaraObscura/HelpSource/images".standardizePath;
        var dvProf, geoScene, grpModel, platModel, modModel, rirModel, vFdnModel;
        
        File.mkdir(irDir);
        
        dvProf = DarkVelvetProfile.exponential(3, 2, sampleRate: s.sampleRate, seed: 1).load(s);
        geoScene = GeometryReverbScene.shoebox([5, 5, 5], 0.3, sampleRate: s.sampleRate, seed: 1, grid: 2, lateT60: 1).load(s);
        grpModel = GroupedFDNModel.synthetic().load(s);
        platModel = ModalPlateModel.synthetic().load(s);
        modModel = ModalReverbModel.synthetic().load(s);
        rirModel = RIRFDNModel.synthetic().load(s);
        vFdnModel = VelvetFDNModel.synthetic().load(s);
        
        s.sync;
        
        [
            [\DarkVelvetReverb, { DarkVelvetReverb.ar(Impulse.ar(0), dvProf, 1, 2000, 1, 1) }],
            [\GeometryReverb, { GeometryReverb.ar(Impulse.ar(0), geoScene, [2,2,2], [5,5,5], [1,1,1])[0] }],
            [\GroupedFDN, { GroupedFDN.ar(Impulse.ar(0), grpModel)[0] }],
            [\ModalPlate, { ModalPlate.ar(Impulse.ar(0), platModel)[0] }],
            [\ModalReverbBank, { ModalReverbBank.ar(Impulse.ar(0), modModel)[0] }],
            [\RIRFDN, { RIRFDN.ar(Impulse.ar(0), rirModel)[0] }],
            [\VelvetFDN, { VelvetFDN.ar(Impulse.ar(0), vFdnModel)[0] }]
        ].do { |pair|
            var name = pair[0];
            var func = pair[1];
            var buf = Buffer.alloc(s, frames, 1);
            s.sync;
            {
                var sig = func.value;
                RecordBuf.ar(sig, buf, loop: 0, doneAction: 2);
                Line.kr(0, 1, dur, doneAction: 2);
            }.play;
            
            (dur + 0.5).wait;
            buf.write(irDir +/+ "ir_" ++ name ++ ".wav", "WAV", "int16");
            s.sync;
            buf.free;
            ("Rendered " ++ name).postln;
        };
        
        0.exit;
    }).play;
};
)
