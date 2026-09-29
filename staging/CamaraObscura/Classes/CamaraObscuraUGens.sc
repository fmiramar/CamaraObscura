// SPDX-License-Identifier: GPL-3.0-or-later

DarkVelvetReverb : MultiOutUGen {
    *ar {
        |input, profileBuf, channels = 2,
        density = 2000, spread = 1, seed = 0, reset = 0|
        var source = input.asArray;
        if(channels.isInteger.not
            or: { channels < 1 }
            or: { channels > 8 }
            or: { seed.isInteger.not }
            or: { seed < 0 }) {
            Error(
                "DarkVelvetReverb: channels must be a literal integer from 1 to 8 and seed a nonnegative literal integer."
            ).throw
        };
        source = Mix(source) / source.size.sqrt;
        ^this.multiNewList([
            \audio, channels, source, profileBuf.asUGenInput,
            channels, density, spread, seed, reset
        ])
    }

    *kr {
        Error("DarkVelvetReverb is audio-rate only.").throw
    }

    init { |numOutputs ... theInputs|
        inputs = theInputs;
        ^this.initOutputs(numOutputs, \audio)
    }

    argNamesInputsOffset { ^2 }
    checkInputs { ^this.checkValidInputs }
}

GroupedFDN : MultiOutUGen {
    *ar {
        |input, modelBuf, groups = 2, delaysPerGroup = 4,
        coupling = 1, freeze = 0, reset = 0|
        if(groups.isInteger.not
            or: { delaysPerGroup.isInteger.not }
            or: { groups < 1 }
            or: { groups > 8 }
            or: { delaysPerGroup < 1 }
            or: { (groups * delaysPerGroup) > 32 }) {
            Error(
                "GroupedFDN: groups and delaysPerGroup must be fixed positive literal integers with at most 32 total lines."
            ).throw
        };
        ^this.multiNewList([
            \audio, 2, input, modelBuf.asUGenInput,
            groups, delaysPerGroup, coupling, freeze, reset
        ])
    }

    *kr { Error("GroupedFDN is audio-rate only.").throw }

    init { |numOutputs ... theInputs|
        inputs = theInputs;
        ^this.initOutputs(numOutputs, \audio)
    }

    argNamesInputsOffset { ^2 }
    checkInputs { ^this.checkValidInputs }
}

VelvetFDN : MultiOutUGen {
    *ar {
        |input, modelBuf, delayCount = 8,
        density = 1, diffusion = 1,
        freeze = 0, reset = 0|
        if(delayCount.isInteger.not
            or: { delayCount < 2 }
            or: { delayCount > 32 }) {
            Error(
                "VelvetFDN: delayCount must be a fixed literal integer from 2 through 32."
            ).throw
        };
        ^this.multiNewList([
            \audio, 2, input, modelBuf.asUGenInput,
            delayCount, density, diffusion, freeze, reset
        ])
    }

    *kr { Error("VelvetFDN is audio-rate only.").throw }

    init { |numOutputs ... theInputs|
        inputs = theInputs;
        ^this.initOutputs(numOutputs, \audio)
    }

    argNamesInputsOffset { ^2 }
    checkInputs { ^this.checkValidInputs }
}

RIRFDN : MultiOutUGen {
    *ar {
        |input, modelBuf, morphBuf = -1, morph = 0,
        decayScale = 1, tone = 0, freeze = 0, reset = 0|
        ^this.multiNewList([
            \audio, 2, input, modelBuf.asUGenInput,
            morphBuf.asUGenInput, morph, decayScale,
            tone, freeze, reset
        ])
    }

    *kr { Error("RIRFDN is audio-rate only.").throw }

    init { |numOutputs ... theInputs|
        inputs = theInputs;
        ^this.initOutputs(numOutputs, \audio)
    }

    argNamesInputsOffset { ^2 }
    checkInputs { ^this.checkValidInputs }
}

ModalReverbBank : MultiOutUGen {
    *ar {
        |input, modelBuf, modeCount = 512,
        pitch = 1, decayScale = 1, dampingTilt = 0,
        dispersion = 0, drive = 0, freeze = 0, reset = 0|
        if(modeCount.isInteger.not
            or: { modeCount < 1 }
            or: { modeCount > 4096 }) {
            Error(
                "ModalReverbBank: modeCount must be a fixed literal integer from 1 through 4096."
            ).throw
        };
        ^this.multiNewList([
            \audio, 2, input, modelBuf.asUGenInput,
            modeCount, pitch, decayScale, dampingTilt,
            dispersion, drive, freeze, reset
        ])
    }

    *kr {
        Error("ModalReverbBank is audio-rate only.").throw
    }

    init { |numOutputs ... theInputs|
        inputs = theInputs;
        ^this.initOutputs(numOutputs, \audio)
    }

    argNamesInputsOffset { ^2 }
    checkInputs { ^this.checkValidInputs }
}

ModalPlate : UGen {
    *ar {
        |input, modelBuf,
        excitationX = 0.3, excitationY = 0.4,
        pickupX = 0.7, pickupY = 0.6,
        pitch = 1, dampingScale = 1,
        drive = 0, reset = 0|
        ^this.multiNew(
            \audio, input, modelBuf.asUGenInput,
            excitationX, excitationY,
            pickupX, pickupY, pitch,
            dampingScale, drive, reset
        )
    }

    *kr { Error("ModalPlate is audio-rate only.").throw }
    checkInputs { ^this.checkValidInputs }
}

GeometryReverb : MultiOutUGen {
    *ar {
        |input, sceneBuf,
        sourcePosition = #[1, 1, 1],
        listenerPosition = #[2, 2, 1],
        sourceRotation = #[0, 0, 0],
        listenerRotation = #[0, 0, 0],
        earlyLevel = 1, lateLevel = 1, reset = 0|
        var source = sourcePosition.asArray.flat;
        var listener = listenerPosition.asArray.flat;
        var sourceAngles = sourceRotation.asArray.flat;
        var listenerAngles = listenerRotation.asArray.flat;
        if(source.size != 3
            or: { listener.size != 3 }
            or: { sourceAngles.size != 3 }
            or: { listenerAngles.size != 3 }) {
            Error(
                "GeometryReverb: positions and rotations must each contain exactly three values."
            ).throw
        };
        ^this.multiNewList(
            [\audio, 2, input, sceneBuf.asUGenInput]
            ++ source ++ listener
            ++ sourceAngles ++ listenerAngles
            ++ [earlyLevel, lateLevel, reset]
        )
    }

    *kr { Error("GeometryReverb is audio-rate only.").throw }

    init { |numOutputs ... theInputs|
        inputs = theInputs;
        ^this.initOutputs(numOutputs, \audio)
    }

    argNamesInputsOffset { ^2 }
    checkInputs { ^this.checkValidInputs }
}
