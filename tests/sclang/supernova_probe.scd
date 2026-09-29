(
var path = PathName(thisProcess.nowExecutingPath)
    .pathOnly +/+ "family_probe.scd";
Server.supernova;
thisProcess.interpreter.executeFile(path);
)
