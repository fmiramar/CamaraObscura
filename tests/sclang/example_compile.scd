(
var directory = PathName(thisProcess.nowExecutingPath)
    .pathOnly.dirname.dirname +/+ "examples";
var files = PathName(directory).files.select { |path|
    path.extension == "scd"
};

files.do { |path|
    var source = File.readAllString(path.fullPath);
    if(source.compile.isNil) {
        Error(
            "Example did not compile: " ++ path.fileName
        ).throw
    };
    ("EXAMPLE_COMPILED " ++ path.fileName).postln
};
"CREATIVE_REVERBS_EXAMPLES_COMPILE_OK".postln;
0.exit
)
