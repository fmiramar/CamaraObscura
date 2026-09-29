(
var pages = [
    "Classes/DarkVelvetReverb",
    "Classes/DarkVelvetProfile",
    "Classes/GroupedFDN",
    "Classes/GroupedFDNModel",
    "Classes/VelvetFDN",
    "Classes/VelvetFDNModel",
    "Classes/RIRFDN",
    "Classes/RIRFDNModel",
    "Classes/ModalReverbBank",
    "Classes/ModalReverbModel",
    "Classes/ModalPlate",
    "Classes/ModalPlateModel",
    "Classes/GeometryReverb",
    "Classes/GeometryReverbScene",
    "Guides/CamaraObscura"
];
var ok = true;

SCDoc.verbosity = 2;
SCDoc.indexAllDocuments(true);
pages.do { |path|
    var document = SCDoc.documents[path];
    if(document.isNil) {
        ("MISSING_DOC " ++ path).postln;
        ok = false
    } {
        var root = SCDoc.parseDoc(document);
        if(root.isNil) {
            ("PARSE_FAILED " ++ path).postln;
            ok = false
        } {
            SCDoc.renderer.renderToFile(
                document.destPath, document, root
            );
            ("RENDERED " ++ path).postln
        }
    }
};
if(ok) {
    "CREATIVE_REVERBS_SCDOC_OK".postln;
    0.exit
} {
    "CREATIVE_REVERBS_SCDOC_FAILED".postln;
    1.exit
};
)
