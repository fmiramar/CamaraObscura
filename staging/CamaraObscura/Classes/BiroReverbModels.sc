// SPDX-License-Identifier: GPL-3.0-or-later

BiroReverbRandom : Object {
    var state;

    *new { |seed = 0|
        ^super.new.init(seed)
    }

    init { |seed|
        state = seed.asInteger.abs % 16777216;
        if(state == 0) { state = 10317 };
        ^this
    }

    next {
        state = ((state * 1664525) + 1013904223) % 16777216;
        ^state / 16777216
    }

    bipolar { ^(this.next * 2) - 1 }
    sign { ^if(this.next < 0.5) { -1 } { 1 } }
}

BiroReverbDesign : Object {
    *isFiniteNumber { |value|
        ^value.isNumber
            and: { value.isNaN.not }
            and: { value.abs < inf }
    }

    *isExactInteger { |value, minimum, maximum|
        ^this.isFiniteNumber(value)
            and: { value == value.asInteger }
            and: { value >= minimum }
            and: { value <= maximum }
    }

    *isValidSampleRate { |value|
        ^this.isFiniteNumber(value)
            and: { value >= 8000 }
            and: { value <= 768000 }
    }

    *validateFDNLines {
        |data, lineCount, groupCount, offset = 0|
        var values = data.asArray;
        if(lineCount < 1
            or: { lineCount > 32 }
            or: { groupCount < 1 }
            or: { groupCount > 8 }
            or: {
                values.size
                    < (offset + (lineCount * 8))
            }) {
            ^false
        };
        lineCount.do { |line|
            var start = offset + (line * 8);
            var entry = values.copyRange(start, start + 7);
            if(this.isExactInteger(entry[0], 1, 4000000).not
                or: {
                    this.isExactInteger(
                        entry[1], 0, groupCount - 1
                    ).not
                }
                or: { entry[2] < 0.02 or: { entry[2] > 120 } }
                or: { entry[3] < 0.02 or: { entry[3] > 120 } }
                or: {
                    entry.copyRange(2, 7).any { |value|
                        this.isFiniteNumber(value).not
                    }
                }
                or: { entry[7] < 10 or: { entry[7] > 100000 } }) {
                ^false
            }
        };
        ^true
    }

    *nextPrime { |value|
        var candidate = value.asInteger.max(2);
        var prime;
        if(candidate.even and: { candidate != 2 }) {
            candidate = candidate + 1
        };
        loop {
            prime = true;
            (2..candidate.sqrt.floor).do { |divisor|
                if((candidate % divisor) == 0) {
                    prime = false
                }
            };
            if(prime) { ^candidate };
            candidate = candidate + 2
        }
    }

    *fdnLines {
        |lineCount, groups, lowT60s, highT60s,
        sampleRate = 48000, seed = 0,
        delayRange = #[0.021, 0.089]|
        var random = BiroReverbRandom(seed);
        var linesPerGroup = lineCount.div(groups);
        if(this.isExactInteger(lineCount, 1, 32).not
            or: { this.isExactInteger(groups, 1, 8).not }
            or: { (lineCount % groups) != 0 }
            or: { lowT60s.size != groups }
            or: { highT60s.size != groups }
            or: { this.isValidSampleRate(sampleRate).not }
            or: { delayRange.size < 2 }
            or: {
                delayRange.copyRange(0, 1).any { |value|
                    this.isFiniteNumber(value).not
                }
            }
            or: { delayRange[0] <= 0 }
            or: { delayRange[1] <= delayRange[0] }
            or: { delayRange[1] > 4 }
            or: {
                (lowT60s ++ highT60s).any { |value|
                    this.isFiniteNumber(value).not
                        or: { value < 0.02 }
                        or: { value > 120 }
                }
            }) {
            Error("BiroReverbDesign: invalid grouped line layout.").throw
        };
        ^Array.fill(lineCount, { |line|
            var group = line.div(linesPerGroup);
            var fraction = (line + random.next) / lineCount;
            var seconds = delayRange[0]
                + (
                    (delayRange[1] - delayRange[0])
                    * fraction
                );
            var delay = this.nextPrime((seconds * sampleRate).round);
            var pan = if(line.even) { -1 } { 1 };
            [
                delay, group,
                lowT60s[group].asFloat,
                highT60s[group].asFloat,
                (if(line.even) { 1 } { -1 })
                    / lineCount.sqrt,
                (1 - (pan * 0.35)) / lineCount.sqrt,
                (1 + (pan * 0.35)) / lineCount.sqrt,
                3500 + (random.next * 4500)
            ]
        }).flat
    }

    *readSoundFile { |path, maximumSeconds = 30|
        var soundFile = SoundFile.new;
        var frames, channels, sampleRate, data;
        if(soundFile.openRead(path).not) {
            Error("Could not open audio file: " ++ path).throw
        };
        frames = soundFile.numFrames.min(
            (soundFile.sampleRate * maximumSeconds).asInteger
        );
        channels = soundFile.numChannels;
        sampleRate = soundFile.sampleRate;
        data = FloatArray.newClear(frames * channels);
        soundFile.readData(data);
        soundFile.close;
        ^[
            data, frames, channels,
            sampleRate
        ]
    }

    *estimateT60 { |samples, channels, sampleRate, startTime = 0.08|
        var start = (startTime * sampleRate).asInteger
            .clip(0, samples.size.div(channels) - 1);
        var frameCount = samples.size.div(channels);
        var window = (sampleRate * 0.05).asInteger.max(32);
        var points = Array.new;
        var maximum = 0.0;
        (start, start + window .. frameCount - 1).do { |frame|
            var sum = 0.0;
            var count = (frameCount - frame).min(window);
            count.do { |offset|
                var mono = 0.0;
                channels.do { |channel|
                    mono = mono + samples[
                        ((frame + offset) * channels) + channel
                    ]
                };
                mono = mono / channels;
                sum = sum + mono.squared
            };
            sum = (sum / count.max(1)).sqrt;
            maximum = maximum.max(sum);
            points = points.add([
                (frame - start) / sampleRate, sum
            ])
        };
        if(maximum <= 1e-12) { ^1.0 };
        points = points.collect { |point|
            [point[0], (point[1] / maximum).max(1e-9).ampdb]
        }.select { |point|
            point[1] <= -5 and: { point[1] >= -55 }
        };
        if(points.size < 2) { ^1.0 };
        {
            var meanX = points.sum { |point| point[0] } / points.size;
            var meanY = points.sum { |point| point[1] } / points.size;
            var numerator = 0.0;
            var denominator = 0.0;
            var slope;
            points.do { |point|
                numerator = numerator
                    + ((point[0] - meanX) * (point[1] - meanY));
                denominator = denominator
                    + (point[0] - meanX).squared
            };
            slope = numerator / denominator.max(1e-12);
            ^(-60 / slope.min(-0.01)).clip(0.05, 120)
        }.value
    }
}

BiroReverbPreparedModel : Object {
    var <magic, <version = 1, <sampleRate, <channels;
    var <flags, <seed, <fieldA, <fieldB, <fieldC, <fieldD;
    var <payload, <buffer;

    initModel {
        |argMagic, argSampleRate, argChannels,
        argFlags, argSeed, fields, argPayload|
        magic = argMagic.asInteger;
        sampleRate = argSampleRate.asFloat;
        channels = argChannels.asInteger;
        flags = argFlags.asInteger;
        seed = argSeed.asInteger.abs % 16777216;
        fieldA = fields[0].asInteger;
        fieldB = fields[1].asInteger;
        fieldC = fields[2].asInteger;
        fieldD = fields[3].asInteger;
        payload = argPayload.asArray.flat.as(FloatArray);
        this.validate;
        ^this
    }

    initSerialized { |data, expectedMagic|
        var values = data.asArray;
        if(values.size < 12
            or: {
                BiroReverbDesign.isExactInteger(
                    values[0], expectedMagic, expectedMagic
                ).not
            }
            or: {
                BiroReverbDesign.isExactInteger(
                    values[1], 1, 1
                ).not
            }
            or: {
                BiroReverbDesign.isExactInteger(
                    values[2], 12, 12
                ).not
            }
            or: {
                BiroReverbDesign.isExactInteger(
                    values[3], values.size, values.size
                ).not
            }) {
            Error("Invalid or unsupported CamaraObscura model file.").throw
        };
        ^this.initModel(
            expectedMagic, values[4], values[5],
            values[6], values[7], values[8..11],
            values[12..]
        )
    }

    validate {
        if(version != 1
            or: {
                BiroReverbDesign.isExactInteger(
                    magic, 1, 16777215
                ).not
            }
            or: {
                BiroReverbDesign.isValidSampleRate(
                    sampleRate
                ).not
            }
            or: { channels < 1 }
            or: { channels > 64 }
            or: { flags < 0 }
            or: { flags >= 16777216 }
            or: { seed < 0 }
            or: { seed >= 16777216 }
            or: { [fieldA, fieldB, fieldC, fieldD].any {
                |value| value < 0 or: { value >= 16777216 }
            } }
            or: { payload.any { |value|
                BiroReverbDesign.isFiniteNumber(value).not
            } }) {
            Error("CamaraObscura prepared model validation failed.").throw
        };
        ^true
    }

    asData {
        var data = [
            magic, version, 12, 12 + payload.size,
            sampleRate, channels, flags, seed,
            fieldA, fieldB, fieldC, fieldD
        ] ++ payload;
        ^data.as(FloatArray)
    }

    load { |server|
        server = server ? Server.default;
        this.validate;
        buffer = Buffer.loadCollection(server, this.asData, 1);
        ^buffer
    }

    free {
        if(buffer.notNil) { buffer.free };
        buffer = nil
    }

    asUGenInput {
        if(buffer.isNil) {
            Error("Load this prepared model before using it in a SynthDef.").throw
        };
        ^buffer.asUGenInput
    }

    summary {
        ^"% v%: % values, % Hz, % channels, seed %"
        .format(
            this.class.name, version, payload.size,
            sampleRate, channels, seed
        )
    }

    write { |path|
        File.use(path.standardizePath, "w", { |file|
            file.write("CREATIVE_REVERBS_MODEL\n");
            this.asData.do { |value|
                file.write(value.asString);
                file.write("\n")
            }
        });
        ^path
    }

    *readData { |path|
        var lines = File.readAllString(path.standardizePath)
            .split(Char.nl)
            .select { |line| line.size > 0 };
        if(lines.isEmpty
            or: { lines[0] != "CREATIVE_REVERBS_MODEL" }) {
            Error("Not a CamaraObscura model file.").throw
        };
        ^lines[1..].collect(_.asFloat).as(FloatArray)
    }
}

DarkVelvetProfile : BiroReverbPreparedModel {
    var <times, <levels, <probabilities;

    *exponential {
        |duration, t60, bands = 8,
        sampleRate = 48000, seed = 0|
        if(BiroReverbDesign.isFiniteNumber(duration).not
            or: { BiroReverbDesign.isFiniteNumber(t60).not }
            or: { duration <= 0 }
            or: { duration > 30 }
            or: { t60 <= 0 }) {
            Error("DarkVelvetProfile: duration and t60 must be positive.").throw
        };
        ^this.envelope(
            [0, duration],
            [0, -60 * duration / t60],
            nil, sampleRate, seed
        )
    }

    *envelope {
        |times, levels, filters,
        sampleRate = 48000, seed = 0|
        var timeValues = times.asArray.collect(_.asFloat);
        var levelValues = levels.asArray.collect(_.asFloat);
        var segmentCount, probabilityValues, payload;
        if(timeValues.size < 2
            or: { timeValues.size != levelValues.size }
            or: {
                BiroReverbDesign.isValidSampleRate(
                    sampleRate
                ).not
            }
            or: { timeValues.any { |value|
                BiroReverbDesign.isFiniteNumber(value).not
            } }
            or: { levelValues.any { |value|
                BiroReverbDesign.isFiniteNumber(value).not
            } }
            or: { timeValues[0] != 0 }
            or: { timeValues.differentiate[1..].any(_ <= 0) }
            or: { timeValues.last > 30 }
            or: { (timeValues.last * sampleRate).round < 1 }) {
            Error(
                "DarkVelvetProfile: times must rise from zero to at most 30 seconds."
            ).throw
        };
        segmentCount = timeValues.size - 1;
        probabilityValues = if(filters.isNil) {
            Array.fill(segmentCount, { |index|
                var position = index / (segmentCount - 1).max(1);
                [
                    0.15 + (0.55 * position),
                    0.08 + (0.12 * position),
                    0.16,
                    0.12 + (0.10 * position),
                    0.32 - (0.24 * position),
                    0.17 - (0.09 * position)
                ].normalizeSum
            })
        } {
            var values = filters.asArray;
            if(values.size == 6
                and: { values[0].isNumber }) {
                values = values ! segmentCount
            };
            if(values.size != segmentCount
                or: { values.any { |row|
                    row.asArray.size != 6
                        or: { row.asArray.any { |value|
                            BiroReverbDesign
                                .isFiniteNumber(value).not
                        } }
                        or: { row.asArray.any(_ < 0) }
                        or: { row.asArray.sum <= 0 }
                } }) {
                Error(
                    "DarkVelvetProfile: filters must provide six probabilities per segment."
                ).throw
            };
            values.collect { |row| row.asArray.normalizeSum }
        };
        payload = Array.fill(segmentCount, { |index|
            [
                timeValues[index], timeValues[index + 1],
                levelValues[index], levelValues[index + 1],
                1
            ] ++ probabilityValues[index]
        }).flat;
        ^super.new.initDark(
            timeValues, levelValues, probabilityValues,
            sampleRate, seed, payload
        )
    }

    *fromIR { |buffer, startTime = 0.08|
        if(buffer.path.isNil) {
            Error(
                "DarkVelvetProfile.fromIR requires a Buffer loaded from a file; use fromFile for explicit paths."
            ).throw
        };
        ^this.fromFile(buffer.path, startTime)
    }

    *fromFile { |path, startTime = 0.08|
        var result = BiroReverbDesign.readSoundFile(path);
        var data = result[0], frames = result[1];
        var channels = result[2], rate = result[3];
        var start = (startTime * rate).asInteger.clip(0, frames - 1);
        var window = (rate * 0.05).asInteger.max(32);
        var timeValues = List[0.0];
        var rmsValues = List.new;
        var maximum;
        (start, start + window .. frames - 1).do { |frame|
            var sum = 0.0;
            var count = (frames - frame).min(window);
            count.do { |offset|
                var mono = 0.0;
                channels.do { |channel|
                    mono = mono + data[
                        ((frame + offset) * channels) + channel
                    ]
                };
                mono = mono / channels;
                sum = sum + mono.squared
            };
            rmsValues.add((sum / count.max(1)).sqrt);
            if(frame > start) {
                timeValues.add((frame - start) / rate)
            }
        };
        if(rmsValues.size < 2 or: { rmsValues.maxItem <= 1e-12 }) {
            Error("DarkVelvetProfile: IR contains no usable late tail.").throw
        };
        maximum = rmsValues.maxItem;
        ^this.envelope(
            timeValues.asArray.copyRange(0, rmsValues.size - 1),
            rmsValues.collect { |value|
                (value / maximum).max(1e-9).ampdb
            }.asArray,
            nil, rate, 0
        )
    }

    initDark {
        |argTimes, argLevels, argProbabilities,
        argSampleRate, argSeed, argPayload|
        times = argTimes;
        levels = argLevels;
        probabilities = argProbabilities;
        ^this.initModel(
            444001, argSampleRate, 8, 0, argSeed,
            [
                argTimes.size - 1, 6,
                (argTimes.last * argSampleRate).round,
                2000
            ],
            argPayload
        )
    }

    validate {
        var previousEnd = 0.0;
        super.validate;
        if(magic != 444001
            or: { channels < 1 or: { channels > 8 } }
            or: { fieldA < 1 or: { fieldA > 256 } }
            or: { fieldB != 6 }
            or: {
                fieldC < 1
                    or: {
                        fieldC
                            > (sampleRate * 30).round
                    }
            }
            or: { fieldD < 20 or: { fieldD > 8000 } }
            or: { payload.size != (fieldA * 11) }) {
            Error("DarkVelvetProfile: invalid prepared payload.").throw
        };
        fieldA.do { |segment|
            var start = segment * 11;
            var entry = payload.copyRange(start, start + 10);
            var probabilities = entry.copyRange(5, 10);
            if((entry[0] - previousEnd).abs > 1e-3
                or: { entry[1] <= entry[0] }
                or: {
                    entry[1]
                        > ((fieldC / sampleRate) + 1e-3)
                }
                or: { entry[4] <= 0 or: { entry[4] > 16 } }
                or: { probabilities.any(_ < 0) }
                or: { probabilities.sum <= 0 }) {
                Error(
                    "DarkVelvetProfile: invalid segment data."
                ).throw
            };
            previousEnd = entry[1]
        };
        if((previousEnd - (fieldC / sampleRate)).abs > 1e-3) {
            Error(
                "DarkVelvetProfile: segment duration mismatch."
            ).throw
        };
        ^true
    }

    initSerialized { |data, expectedMagic = 444001|
        super.initSerialized(data, expectedMagic);
        times = [0];
        levels = [payload[2]];
        probabilities = Array.fill(fieldA, { |index|
            var offset = index * 11;
            if(index > 0) {
                times = times.add(payload[offset])
            };
            levels = levels.add(payload[offset + 3]);
            payload.copyRange(offset + 5, offset + 10)
        });
        times = times.add(fieldC / sampleRate);
        ^this
    }

    *read { |path|
        ^super.new.initSerialized(this.readData(path), 444001)
    }

    plotEnvelope { ^levels.plot("DarkVelvet broadband envelope") }
    plotBandEnergy {
        ^probabilities.flop.collect(_.asArray)
            .plot("DarkVelvet dictionary probabilities")
    }
}

GroupedFDNModel : BiroReverbPreparedModel {
    var <groupSpecs, <couplingMatrix;

    *twoRooms {
        |smallT60, largeT60, aperture,
        sampleRate = 48000, seed = 0|
        ^this.fromGroups(
            [
                [smallT60, smallT60 * 0.65, 0.018, 0.052, 4],
                [largeT60, largeT60 * 0.7, 0.047, 0.113, 4]
            ],
            [[1, aperture], [aperture, 1]],
            sampleRate, seed
        )
    }

    *fromGroups {
        |groupSpecs, couplingMatrix,
        sampleRate = 48000, seed = 0|
        var specs = groupSpecs.asArray.collect(_.asArray);
        var groups = specs.size;
        var linesPerGroup, lineCount, pairAngles;
        var matrixValid = true;
        var lineData, payload;
        if(groups < 1 or: { groups > 8 }
            or: {
                BiroReverbDesign.isValidSampleRate(
                    sampleRate
                ).not
            }
            or: { specs.any { |spec|
                spec.size < 5
                    or: {
                        spec.copyRange(0, 3).any { |value|
                            BiroReverbDesign
                                .isFiniteNumber(value).not
                        }
                    }
            } }) {
            Error("GroupedFDNModel: invalid group specifications.").throw
        };
        linesPerGroup = specs[0][4].asInteger;
        if(linesPerGroup < 1
            or: { specs.any { |spec|
                BiroReverbDesign.isExactInteger(
                    spec[4], 1, 32
                ).not
                    or: { spec[4].asInteger != linesPerGroup }
                    or: { spec[0] <= 0 }
                    or: { spec[0] > 120 }
                    or: { spec[1] <= 0 }
                    or: { spec[1] > 120 }
                    or: { spec[2] <= 0 }
                    or: { spec[3] <= spec[2] }
                    or: { spec[3] > 4 }
            } }) {
            Error(
                "GroupedFDNModel: groups need equal positive line counts and decay times."
            ).throw
        };
        lineCount = groups * linesPerGroup;
        if(lineCount > 32) {
            Error("GroupedFDNModel: at most 32 delay lines are supported.").throw
        };
        couplingMatrix = couplingMatrix.asArray.collect(_.asArray);
        if(couplingMatrix.size == groups
            and: {
                couplingMatrix.every { |row|
                    row.size == groups
                }
            }) {
            groups.do { |row|
                groups.do { |column|
                    var first = couplingMatrix[row][column];
                    var second = couplingMatrix[column][row];
                    if(BiroReverbDesign.isFiniteNumber(first).not
                        or: { first.abs > 1 }
                        or: { (first - second).abs > 1e-6 }
                        or: {
                            row == column
                                and: { (first - 1).abs > 1e-6 }
                        }) {
                        matrixValid = false
                    }
                }
            }
        } {
            matrixValid = false
        };
        if(matrixValid.not) {
            Error(
                "GroupedFDNModel: couplingMatrix must be finite, square, and symmetric."
            ).throw
        };
        pairAngles = List.new;
        (groups - 1).do { |first|
            (first + 1 .. groups - 1).do { |second|
                pairAngles.add(
                    couplingMatrix[first][second]
                        .abs.clip(0, 1) * (pi / 4)
                )
            }
        };
        lineData = Array.fill(groups, { |group|
            BiroReverbDesign.fdnLines(
                linesPerGroup, 1,
                [specs[group][0]], [specs[group][1]],
                sampleRate,
                (seed + ((group + 1) * 7919)) % 16777216,
                [specs[group][2], specs[group][3]]
            ).clump(8).collect { |entry|
                entry[1] = group;
                entry
            }.flat
        }).flat;
        payload = pairAngles.asArray ++ lineData;
        ^super.new.initGrouped(
            specs, couplingMatrix, sampleRate, seed,
            lineCount, groups, linesPerGroup, payload
        )
    }

    initGrouped {
        |argSpecs, argMatrix, argSampleRate, argSeed,
        lineCount, groups, linesPerGroup, argPayload|
        groupSpecs = argSpecs;
        couplingMatrix = argMatrix;
        ^this.initModel(
            444002, argSampleRate, 2, 0, argSeed,
            [lineCount, groups, linesPerGroup, 8],
            argPayload
        )
    }

    validate {
        var pairCount;
        super.validate;
        pairCount = (fieldB * (fieldB - 1) / 2).asInteger;
        if(magic != 444002
            or: { channels != 2 }
            or: { fieldA < 1 or: { fieldA > 32 } }
            or: { fieldB < 1 or: { fieldB > 8 } }
            or: { fieldC < 1 or: { fieldC > 32 } }
            or: { fieldA != (fieldB * fieldC) }
            or: { fieldD != 8 }
            or: {
                payload.size
                    != (pairCount + (fieldA * 8))
            }
            or: {
                pairCount > 0 and: {
                    payload.copyRange(
                        0, pairCount - 1
                    ).any { |angle|
                        angle < 0 or: { angle > (pi / 2) }
                    }
                }
            }
            or: {
                BiroReverbDesign.validateFDNLines(
                    payload, fieldA, fieldB, pairCount
                ).not
            }) {
            Error("GroupedFDNModel: invalid prepared payload.").throw
        };
        fieldA.do { |line|
            if(payload[pairCount + (line * 8) + 1]
                != line.div(fieldC)) {
                Error(
                    "GroupedFDNModel: line/group order mismatch."
                ).throw
            }
        };
        ^true
    }

    *read { |path|
        ^super.new.initSerialized(this.readData(path), 444002)
    }

    initSerialized { |data, expectedMagic = 444002|
        var pairCount, offset;
        super.initSerialized(data, expectedMagic);
        pairCount = (fieldB * (fieldB - 1) / 2).asInteger;
        couplingMatrix = Array.fill(fieldB, { |row|
            Array.fill(fieldB, { |column|
                if(row == column) { 1.0 } { 0.0 }
            })
        });
        offset = 0;
        (fieldB - 1).do { |first|
            (first + 1 .. fieldB - 1).do { |second|
                var amount = (
                    payload[offset] / (pi / 4)
                ).clip(0, 1);
                couplingMatrix[first][second] = amount;
                couplingMatrix[second][first] = amount;
                offset = offset + 1
            }
        };
        groupSpecs = Array.fill(fieldB, { |group|
            var entry = pairCount
                + (group * fieldC * 8);
            [
                payload[entry + 2], payload[entry + 3],
                payload[entry] / sampleRate,
                payload[
                    entry + ((fieldC - 1) * 8)
                ] / sampleRate,
                fieldC
            ]
        });
        ^this
    }

    plotDecay {
        ^groupSpecs.collect { |spec| [spec[0], spec[1]] }
            .flop.plot("GroupedFDN target T60")
    }

    plotCoupling {
        ^couplingMatrix.flat.plot("GroupedFDN coupling matrix")
    }
}

VelvetFDNModel : BiroReverbPreparedModel {
    var <modeName;

    *default {
        |delayCount = 8, t60 = 3,
        sampleRate = 48000, seed = 0|
        ^this.prBuild(
            delayCount, t60, t60 * 0.65,
            0, 8, sampleRate, seed, nil
        )
    }

    *filteredMatrix {
        |delays, velvetFilters, attenuationFilters,
        feedbackStructure, sampleRate = 48000, seed = 0|
        var delayValues = delays.asArray;
        var lineCount = delayValues.size;
        var t60Low = 3.0, t60High = 1.8;
        var mode = if(feedbackStructure == \paraunitary) { 1 } { 0 };
        if(attenuationFilters.notNil) {
            attenuationFilters = attenuationFilters.asArray.flat;
            if(attenuationFilters.isEmpty
                or: { attenuationFilters.size > 2 }
                or: { attenuationFilters.any { |value|
                    BiroReverbDesign.isFiniteNumber(value).not
                        or: { value <= 0 }
                        or: { value > 120 }
                } }) {
                Error(
                    "VelvetFDNModel: attenuationFilters must contain one or two positive T60 values."
                ).throw
            };
            t60Low = attenuationFilters[0].asFloat;
            t60High = attenuationFilters.wrapAt(1).asFloat
        };
        ^this.prBuild(
            lineCount, t60Low, t60High, mode,
            velvetFilters.asArray.size.clip(1, 16),
            sampleRate, seed, delayValues
        )
    }

    *prBuild {
        |lineCount, t60Low, t60High, mode,
        tapCount, sampleRate, seed, explicitDelays|
        var random = BiroReverbRandom(seed);
        var historySize = (sampleRate * 0.03).asInteger.max(64);
        var lineData, tapData, scatterData = [];
        var payload;
        if(lineCount < 2 or: { lineCount > 32 }
            or: {
                BiroReverbDesign.isValidSampleRate(
                    sampleRate
                ).not
            }
            or: { mode == 1 and: {
                [2, 4, 8, 16, 32].includes(lineCount).not
            } }
            or: { t60Low <= 0 }
            or: { t60Low > 120 }
            or: { t60High <= 0 }
            or: { t60High > 120 }
            or: { tapCount < 1 or: { tapCount > 16 } }
            or: {
                explicitDelays.notNil and: {
                    explicitDelays.size != lineCount
                        or: {
                            explicitDelays.any { |delay|
                                BiroReverbDesign
                                    .isFiniteNumber(delay).not
                                    or: { delay <= 0 }
                                    or: {
                                        delay >= 2
                                            and: {
                                                delay
                                                    > 4000000
                                            }
                                    }
                            }
                        }
                }
            }) {
            Error("VelvetFDNModel: invalid delay count or decay.").throw
        };
        lineData = BiroReverbDesign.fdnLines(
            lineCount, 1, [t60Low], [t60High],
            sampleRate, seed, [0.037, 0.127]
        );
        if(explicitDelays.notNil) {
            explicitDelays.do { |delay, line|
                lineData[line * 8] = if(delay < 2) {
                    BiroReverbDesign.nextPrime(
                        (delay * sampleRate).round
                    )
                } {
                    delay.asInteger
                }
            }
        };
        tapData = Array.fill(lineCount + 2, { |branch|
            Array.fill(tapCount, { |tap|
                var cell = historySize / tapCount;
                var delay = (
                    (tap * cell)
                    + (random.next * cell)
                ).asInteger.clip(0, historySize - 1);
                var gain = random.sign.asFloat;
                [delay, gain]
            })
        }).flat;
        if(mode == 1) {
            scatterData = Array.fill(lineCount, { |line|
                BiroReverbDesign.nextPrime(
                    3 + (line * 5) + (random.next * 11).asInteger
                )
            })
        };
        payload = [pi / 4] ++ lineData ++ tapData ++ scatterData;
        ^super.new.initVelvet(
            mode, sampleRate, seed, lineCount,
            tapCount, historySize, payload
        )
    }

    initVelvet {
        |mode, argSampleRate, argSeed, lineCount,
        tapCount, historySize, argPayload|
        modeName = if(mode == 0) {
            \inputOutput
        } {
            \paraunitaryFeedback
        };
        ^this.initModel(
            444003, argSampleRate, 2, mode, argSeed,
            [lineCount, mode, tapCount, historySize],
            argPayload
        )
    }

    validate {
        var lineOffset = 1;
        var tapOffset, tapValues, scatterOffset;
        var expected;
        super.validate;
        expected = 1 + (fieldA * 8)
            + ((fieldA + 2) * fieldC * 2)
            + if(fieldB == 1) { fieldA } { 0 };
        if(magic != 444003
            or: { channels != 2 }
            or: { fieldA < 2 or: { fieldA > 32 } }
            or: { fieldB < 0 or: { fieldB > 1 } }
            or: { fieldC < 1 or: { fieldC > 16 } }
            or: { fieldD < 64 or: { fieldD > 65536 } }
            or: {
                fieldB == 1 and: {
                    [2, 4, 8, 16, 32].includes(fieldA).not
                }
            }
            or: { payload.size != expected }
            or: { payload[0] < 0 or: { payload[0] > (pi / 2) } }
            or: {
                BiroReverbDesign.validateFDNLines(
                    payload, fieldA, 1, lineOffset
                ).not
            }) {
            Error("VelvetFDNModel: invalid prepared payload.").throw
        };
        tapOffset = 1 + (fieldA * 8);
        tapValues = (fieldA + 2) * fieldC;
        tapValues.do { |tap|
            var start = tapOffset + (tap * 2);
            if(BiroReverbDesign.isExactInteger(
                payload[start], 0, fieldD - 1
            ).not
                or: {
                    BiroReverbDesign.isFiniteNumber(
                        payload[start + 1]
                    ).not
                }) {
                Error(
                    "VelvetFDNModel: invalid sparse filter tap."
                ).throw
            }
        };
        if(fieldB == 1) {
            scatterOffset = tapOffset + (tapValues * 2);
            fieldA.do { |line|
                if(BiroReverbDesign.isExactInteger(
                    payload[scatterOffset + line], 1, 4096
                ).not) {
                    Error(
                        "VelvetFDNModel: invalid scattering delay."
                    ).throw
                }
            }
        };
        ^true
    }

    *read { |path|
        ^super.new.initSerialized(this.readData(path), 444003)
    }

    initSerialized { |data, expectedMagic = 444003|
        super.initSerialized(data, expectedMagic);
        modeName = if(fieldB == 0) {
            \inputOutput
        } {
            \paraunitaryFeedback
        };
        ^this
    }
}

RIRFDNModel : BiroReverbPreparedModel {
    var <sourcePath;

    *read { |path|
        var object = super.new.initSerialized(
            this.readData(path), 444004
        );
        object.sourcePath_(path);
        ^object
    }

    *fromAnalysis { |path| ^this.read(path) }

    *synthetic {
        |t60Low = 3, t60High = 1.8, delays = 8,
        sampleRate = 48000, seed = 0|
        var payload;
        if(BiroReverbDesign.isExactInteger(
            delays, 2, 32
        ).not
            or: {
                BiroReverbDesign.isValidSampleRate(
                    sampleRate
                ).not
            }
            or: {
                BiroReverbDesign.isFiniteNumber(t60Low).not
            }
            or: {
                BiroReverbDesign.isFiniteNumber(t60High).not
            }
            or: { t60Low < 0.02 }
            or: { t60Low > 120 }
            or: { t60High < 0.02 }
            or: { t60High > 120 }) {
            Error("RIRFDNModel: invalid synthetic model request.").throw
        };
        delays = delays.asInteger;
        payload = [pi / 4] ++ BiroReverbDesign.fdnLines(
            delays, 1, [t60Low], [t60High],
            sampleRate, seed, [0.031, 0.109]
        );
        ^super.new.initRIR(
            sampleRate, seed, delays, payload, nil
        )
    }

    validate {
        super.validate;
        if(magic != 444004
            or: { channels != 2 }
            or: { fieldA < 2 or: { fieldA > 32 } }
            or: { fieldB != 1 }
            or: { fieldC != fieldA }
            or: { fieldD != 8 }
            or: { payload.size != (1 + (fieldA * 8)) }
            or: { payload[0] < 0 or: { payload[0] > (pi / 2) } }
            or: {
                BiroReverbDesign.validateFDNLines(
                    payload, fieldA, 1, 1
                ).not
            }) {
            Error("RIRFDNModel: invalid prepared payload.").throw
        };
        ^true
    }

    initRIR {
        |argSampleRate, argSeed, delayCount,
        argPayload, argSourcePath|
        sourcePath = argSourcePath;
        ^this.initModel(
            444004, argSampleRate, 2, 1, argSeed,
            [delayCount, 1, delayCount, 8],
            argPayload
        )
    }

    sourcePath_ { |path| sourcePath = path }

    plotDecay {
        var values = Array.fill(fieldA, { |line|
            var offset = 1 + (line * 8);
            [payload[offset + 2], payload[offset + 3]]
        });
        ^values.flop.plot("RIRFDN fitted T60")
    }

    plotResponse { ^this.plotDecay }
}

ModalReverbModel : BiroReverbPreparedModel {
    var <frequencies, <decays, <gains;

    *fromModes {
        |frequencies, decays, gains, sampleRate = 48000|
        var frequencyValues = frequencies.asArray;
        var decayValues = decays.asArray;
        var gainValues = gains.asArray;
        var payload;
        if(frequencyValues.isEmpty
            or: { frequencyValues.size != decayValues.size }
            or: { frequencyValues.size != gainValues.size }
            or: { frequencyValues.size > 4096 }
            or: {
                BiroReverbDesign.isFiniteNumber(sampleRate).not
                    or: { sampleRate < 8000 }
                    or: { sampleRate > 768000 }
            }
            or: { frequencyValues.any { |value|
                BiroReverbDesign.isFiniteNumber(value).not
                    or: { value <= 0 }
                    or: { value >= (sampleRate * 0.5) }
            } }
            or: { decayValues.any { |value|
                BiroReverbDesign.isFiniteNumber(value).not
                    or: { value <= 0 }
                    or: { value > 120 }
            } }
            or: { gainValues.any { |gain|
                var values = gain.asArray;
                values.isEmpty
                    or: { values.size > 2 }
                    or: { values.any { |value|
                        BiroReverbDesign.isFiniteNumber(value).not
                    } }
            } }) {
            Error("ModalReverbModel: mode arrays must have equal nonzero lengths.").throw
        };
        payload = Array.fill(frequencyValues.size, { |index|
            var gain = gainValues[index].asArray;
            var left = gain[0];
            var right = if(gain.size > 1) {
                gain[1]
            } {
                if(index.even) { left } { left.neg }
            };
            [
                frequencyValues[index], decayValues[index],
                1, left, right, 0
            ]
        }).flat;
        ^super.new.initModal(
            frequencyValues, decayValues, gainValues,
            sampleRate, 0, payload
        )
    }

    *random {
        |count, range, decayRange, seed = 0,
        sampleRate = 48000|
        var rangeValues = range.asArray;
        var decayValues = decayRange.asArray;
        var random, low, high;
        var frequencies, decays, gains;
        if(BiroReverbDesign.isExactInteger(
            count, 1, 4096
        ).not
            or: {
                BiroReverbDesign.isValidSampleRate(
                    sampleRate
                ).not
            }
            or: { rangeValues.size < 2 }
            or: { decayValues.size < 2 }
            or: {
                (rangeValues.copyRange(0, 1)
                    ++ decayValues.copyRange(0, 1))
                .any { |value|
                    BiroReverbDesign.isFiniteNumber(value).not
                }
            }) {
            Error("ModalReverbModel: invalid random model request.").throw
        };
        random = BiroReverbRandom(seed);
        low = rangeValues[0].max(1);
        high = rangeValues[1].min(sampleRate * 0.49);
        if(high <= low
            or: { decayValues[0] < 0.005 }
            or: { decayValues[1] < decayValues[0] }
            or: { decayValues[1] > 120 }) {
            Error("ModalReverbModel: invalid random model request.").throw
        };
        count = count.asInteger;
        frequencies = Array.fill(count, {
            low * ((high / low) ** random.next)
        }).sort;
        decays = Array.fill(count, {
            decayValues[0]
                + ((decayValues[1] - decayValues[0]) * random.next)
        });
        gains = Array.fill(count, { random.bipolar / count.sqrt });
        ^this.prFromModes(
            frequencies, decays, gains,
            sampleRate, seed
        )
    }

    *harmonic {
        |fundamental, count, decay,
        sampleRate = 48000|
        var frequencies, decays, gains;
        if(BiroReverbDesign.isFiniteNumber(fundamental).not
            or: {
                BiroReverbDesign.isExactInteger(
                    count, 1, 4096
                ).not
            }
            or: { BiroReverbDesign.isFiniteNumber(decay).not }
            or: {
                BiroReverbDesign.isValidSampleRate(
                    sampleRate
                ).not
            }
            or: { fundamental <= 0 }
            or: { fundamental >= (sampleRate * 0.49) }
            or: { decay < 0.005 }
            or: { decay > 120 }) {
            Error(
                "ModalReverbModel: invalid harmonic model request."
            ).throw
        };
        count = count.asInteger;
        frequencies = Array.fill(count, { |index|
            fundamental * (index + 1)
        }).select(_ < (sampleRate * 0.49));
        decays = decay ! frequencies.size;
        gains = Array.fill(frequencies.size, { |index|
            (if(index.even) { 1 } { -1 })
                / ((index + 1).sqrt * frequencies.size.sqrt)
        });
        ^this.prFromModes(
            frequencies, decays, gains,
            sampleRate, 0
        )
    }

    *fromIR { |path, modes = 512|
        var project = this.filenameSymbol.asString
            .dirname.dirname;
        var tool = project +/+
            "tools/modal_extract/modal_extract.py";
        var output = PathName.tmp +/+
            ("creative_reverbs_modal_"
                ++ UniqueID.next.asString ++ ".modal");
        var report = output ++ ".report.txt";
        var python = if(Platform.name == \windows) {
            "python"
        } {
            "python3"
        };
        var command, model;
        if(BiroReverbDesign.isExactInteger(
            modes, 1, 4096
        ).not) {
            Error(
                "ModalReverbModel.fromIR modes must be an integer from 1 through 4096."
            ).throw
        };
        if(File.exists(tool).not) {
            Error(
                "ModalReverbModel.fromIR cannot find modal_extract.py."
            ).throw
        };
        command = "% % analyze % % --modes % --report %"
        .format(
            python, tool.shellQuote,
            path.standardizePath.shellQuote,
            output.shellQuote, modes.asInteger,
            report.shellQuote
        );
        command.unixCmdGetStdOut;
        if(File.exists(output).not) {
            Error(
                "ModalReverbModel.fromIR analysis failed."
            ).throw
        };
        model = this.read(output);
        File.delete(output);
        if(File.exists(report)) { File.delete(report) };
        ^model
    }

    *fromAnalysis { |path| ^this.read(path) }

    *prFromModes {
        |frequencyValues, decayValues, gainValues,
        sampleRate, seed|
        var payload = Array.fill(frequencyValues.size, { |index|
            var gain = gainValues[index];
            [
                frequencyValues[index], decayValues[index],
                1, gain, if(index.even) { gain } { gain.neg }, 0
            ]
        }).flat;
        ^super.new.initModal(
            frequencyValues, decayValues, gainValues,
            sampleRate, seed, payload
        )
    }

    initModal {
        |argFrequencies, argDecays, argGains,
        argSampleRate, argSeed, argPayload|
        frequencies = argFrequencies;
        decays = argDecays;
        gains = argGains;
        ^this.initModel(
            444005, argSampleRate, 2, 0, argSeed,
            [argFrequencies.size, 6, 0, 0],
            argPayload
        )
    }

    validate {
        super.validate;
        if(magic != 444005
            or: { channels != 2 }
            or: { fieldA < 1 or: { fieldA > 4096 } }
            or: { fieldB != 6 }
            or: { fieldC != 0 }
            or: { fieldD != 0 }
            or: { payload.size != (fieldA * 6) }) {
            Error("ModalReverbModel: invalid prepared payload.").throw
        };
        fieldA.do { |mode|
            var entry = payload.copyRange(
                mode * 6, (mode * 6) + 5
            );
            if(entry[0] <= 0
                or: { entry[0] >= (sampleRate * 0.5) }
                or: { entry[1] < 0.005 }
                or: { entry[1] > 120 }) {
                Error(
                    "ModalReverbModel: invalid modal frequency or decay."
                ).throw
            }
        };
        ^true
    }

    *read { |path|
        ^super.new.initSerialized(this.readData(path), 444005)
    }

    initSerialized { |data, expectedMagic = 444005|
        super.initSerialized(data, expectedMagic);
        frequencies = Array.fill(fieldA, { |index|
            payload[index * 6]
        });
        decays = Array.fill(fieldA, { |index|
            payload[(index * 6) + 1]
        });
        gains = Array.fill(fieldA, { |index|
            payload[(index * 6) + 3]
        });
        ^this
    }

    plotModes { ^frequencies.plot("ModalReverb frequencies") }
    plotDecay { ^decays.plot("ModalReverb T60") }
}

ModalPlateModel : BiroReverbPreparedModel {
    var <width, <height, <thickness, <density;
    var <youngModulus, <poissonRatio, <damping;
    var <boundary, <modePairs, <frequencies;

    *rectangular {
        |width, height, thickness, density,
        youngModulus, poissonRatio, damping,
        modes = 512, boundary = \simplySupported,
        sampleRate = 48000|
        var rigidity, waveFactor, candidates;
        var searchSize, selected, payload;
        if([
            width, height, thickness, density,
            youngModulus, poissonRatio, damping
        ].any { |value|
            BiroReverbDesign.isFiniteNumber(value).not
        }
            or: {
                BiroReverbDesign.isValidSampleRate(
                    sampleRate
                ).not
            }
            or: { width <= 0 } or: { height <= 0 }
            or: { thickness <= 0 }
            or: { density <= 0 }
            or: { youngModulus <= 0 }
            or: { poissonRatio <= -0.999 }
            or: { poissonRatio >= 0.499 }
            or: { damping <= 0 }
            or: {
                BiroReverbDesign.isExactInteger(
                    modes, 1, 4096
                ).not
            }
            or: { boundary != \simplySupported }) {
            Error(
                "ModalPlateModel: invalid parameters; only simplySupported boundaries are available."
            ).throw
        };
        modes = modes.asInteger;
        rigidity = youngModulus * thickness.cubed
            / (12 * (1 - poissonRatio.squared));
        waveFactor = (rigidity / (density * thickness)).sqrt;
        searchSize = (modes.sqrt.ceil * 4).asInteger.max(8);
        candidates = List.new;
        searchSize.do { |x|
            searchSize.do { |y|
                var modeX = x + 1;
                var modeY = y + 1;
                var spatial =
                    (modeX.squared / width.squared)
                    + (modeY.squared / height.squared);
                var frequency = (pi / 2) * waveFactor * spatial;
                var t60 = (
                    (3 * 10.log)
                    / (damping * 2pi * frequency)
                ).clip(0.02, 120);
                candidates.add([
                    modeX, modeY, frequency, t60,
                    2 / (width * height).sqrt
                ])
            }
        };
        candidates = candidates.asArray.select { |mode|
            mode[2] < (sampleRate * 0.49)
        }.sort { |a, b| a[2] < b[2] };
        selected = candidates.keep(modes);
        if(selected.size < modes.min(8)) {
            Error(
                "ModalPlateModel: too few modes lie below Nyquist."
            ).throw
        };
        payload = selected.flat;
        ^super.new.initPlate(
            width, height, thickness, density,
            youngModulus, poissonRatio, damping,
            boundary, selected, sampleRate, payload
        )
    }

    initPlate {
        |argWidth, argHeight, argThickness, argDensity,
        argYoung, argPoisson, argDamping, argBoundary,
        modes, argSampleRate, argPayload|
        width = argWidth;
        height = argHeight;
        thickness = argThickness;
        density = argDensity;
        youngModulus = argYoung;
        poissonRatio = argPoisson;
        damping = argDamping;
        boundary = argBoundary;
        modePairs = modes.collect { |mode| mode.copyRange(0, 1) };
        frequencies = modes.collect(_[2]);
        ^this.initModel(
            444006, argSampleRate, 1, 0, 0,
            [modes.size, 5, 1, 0], argPayload
        )
    }

    validate {
        super.validate;
        if(magic != 444006
            or: { channels != 1 }
            or: { fieldA < 1 or: { fieldA > 4096 } }
            or: { fieldB != 5 }
            or: { fieldC != 1 }
            or: { fieldD != 0 }
            or: { payload.size != (fieldA * 5) }) {
            Error("ModalPlateModel: invalid prepared payload.").throw
        };
        fieldA.do { |mode|
            var start = mode * 5;
            if(BiroReverbDesign.isExactInteger(
                payload[start], 1, 4096
            ).not
                or: {
                    BiroReverbDesign.isExactInteger(
                        payload[start + 1], 1, 4096
                    ).not
                }
                or: { payload[start + 2] <= 0 }
                or: {
                    payload[start + 2]
                        >= (sampleRate * 0.5)
                }
                or: { payload[start + 3] < 0.005 }
                or: { payload[start + 3] > 120 }) {
                Error(
                    "ModalPlateModel: invalid prepared mode."
                ).throw
            }
        };
        ^true
    }

    *read { |path|
        ^super.new.initSerialized(this.readData(path), 444006)
    }

    plotModes {
        var values = if(frequencies.notNil) {
            frequencies
        } {
            Array.fill(fieldA, { |index| payload[(index * 5) + 2] })
        };
        ^values.plot("ModalPlate frequencies")
    }

    plotShape { |mode = 0|
        var pair = if(modePairs.notNil) {
            modePairs.wrapAt(mode)
        } {
            [
                payload[(mode.wrap(0, fieldA - 1) * 5)],
                payload[(mode.wrap(0, fieldA - 1) * 5) + 1]
            ]
        };
        ^Array.fill(32, { |x|
            Array.fill(32, { |y|
                sin(pair[0] * pi * x / 31)
                    * sin(pair[1] * pi * y / 31)
            })
        }).plot("ModalPlate mode shape")
    }

    plotResponse { ^this.plotModes }
}

GeometryReverbScene : BiroReverbPreparedModel {
    var <size, <absorption, <gridSize, <lateT60;

    *shoebox {
        |size, absorption,
        sampleRate = 48000, seed = 0,
        grid = 3, lateT60 = 2.5|
        var room = size.asArray.collect(_.asFloat);
        var absorptionValues;
        var points, pathData, lineCount = 8;
        var lineData, payload;
        if(room.size != 3
            or: { room.any { |value|
                BiroReverbDesign.isFiniteNumber(value).not
                    or: { value <= 0 }
                    or: { value > 300 }
            } }
            or: {
                BiroReverbDesign.isValidSampleRate(
                    sampleRate
                ).not
            }
            or: {
                BiroReverbDesign.isExactInteger(
                    grid, 2, 4
                ).not
            }
            or: {
                BiroReverbDesign.isFiniteNumber(
                    lateT60
                ).not
            }
            or: { lateT60 <= 0 }
            or: { lateT60 > 120 }) {
            Error("GeometryReverbScene: invalid shoebox parameters.").throw
        };
        grid = grid.asInteger;
        absorptionValues = if(absorption.isNumber) {
            absorption.asFloat ! 6
        } {
            absorption.asArray.collect(_.asFloat)
        };
        if(absorptionValues.size != 6
            or: { absorptionValues.any { |value|
                BiroReverbDesign.isFiniteNumber(value).not
                    or: { value < 0 }
                    or: { value > 1 }
            } }) {
            Error(
                "GeometryReverbScene: absorption must be one value or six wall values from zero to one."
            ).throw
        };
        points = Array.fill(grid.cubed.asInteger, { |index|
            var z = index % grid;
            var y = index.div(grid) % grid;
            var x = index.div(grid.squared);
            [
                room[0] * x / (grid - 1),
                room[1] * y / (grid - 1),
                room[2] * z / (grid - 1)
            ]
        });
        pathData = Array.new;
        points.do { |source|
            points.do { |listener|
                var directDelta = source - listener;
                var directDistance = directDelta
                    .sum(_.squared).sqrt.max(0.2);
                pathData = pathData.add([
                    directDistance / 343,
                    1 / directDistance,
                    directDelta[0] / directDistance,
                    directDelta[1] / directDistance,
                    (sampleRate * 0.45).min(20000),
                    0, 0
                ]);
                6.do { |wall|
                    var image = source.copy;
                    var delta, distance, reflection;
                    switch(wall,
                        0, { image[0] = image[0].neg },
                        1, { image[0] = (2 * room[0]) - image[0] },
                        2, { image[1] = image[1].neg },
                        3, { image[1] = (2 * room[1]) - image[1] },
                        4, { image[2] = image[2].neg },
                        5, { image[2] = (2 * room[2]) - image[2] }
                    );
                    delta = image - listener;
                    distance = delta.sum(_.squared).sqrt.max(0.2);
                    reflection = (1 - absorptionValues[wall]).sqrt;
                    pathData = pathData.add([
                        distance / 343,
                        reflection / distance,
                        delta[0] / distance,
                        delta[1] / distance,
                        (1000 + (17000 * reflection))
                            .min(sampleRate * 0.45),
                        1, 1
                    ])
                }
            }
        };
        lineData = BiroReverbDesign.fdnLines(
            lineCount, 1, [lateT60],
            [lateT60 * 0.55],
            sampleRate, seed, [0.043, 0.131]
        );
        payload = room ++ [343] ++ pathData.flat
            ++ [pi / 4] ++ lineData;
        ^super.new.initScene(
            room, absorptionValues, grid,
            lateT60, sampleRate, seed,
            lineCount, payload
        )
    }

    *fromMesh { |path, materials|
        Error(
            "GeometryReverbScene.fromMesh is intentionally unavailable until mesh validation and path pruning are implemented."
        ).throw
    }

    *read { |path|
        ^super.new.initSerialized(this.readData(path), 444007)
    }

    initSerialized { |data, expectedMagic = 444007|
        super.initSerialized(data, expectedMagic);
        size = payload.copyRange(0, 2);
        gridSize = fieldA;
        absorption = nil;
        lateT60 = nil;
        ^this
    }

    initScene {
        |argSize, argAbsorption, argGrid,
        argLateT60, argSampleRate, argSeed,
        lineCount, argPayload|
        size = argSize;
        absorption = argAbsorption;
        gridSize = argGrid;
        lateT60 = argLateT60;
        ^this.initModel(
            444007, argSampleRate, 2, 0, argSeed,
            [argGrid, 7, lineCount, 8],
            argPayload
        )
    }

    validate {
        var points, combinations, pathSamples;
        var lateOffset;
        super.validate;
        points = fieldA.cubed.asInteger;
        combinations = points.squared;
        pathSamples = combinations * fieldB * 7;
        lateOffset = 4 + pathSamples;
        if(magic != 444007
            or: { channels != 2 }
            or: { fieldA < 2 or: { fieldA > 4 } }
            or: { fieldB != 7 }
            or: { fieldC < 2 or: { fieldC > 32 } }
            or: { fieldD != 8 }
            or: {
                payload.size
                    != (lateOffset + 1 + (fieldC * 8))
            }
            or: { payload[0] <= 0 }
            or: { payload[1] <= 0 }
            or: { payload[2] <= 0 }
            or: { payload[3] < 250 or: { payload[3] > 400 } }) {
            Error(
                "GeometryReverbScene: invalid prepared payload."
            ).throw
        };
        (combinations * fieldB).do { |path|
            var start = 4 + (path * 7);
            if(payload[start] <= 0
                or: { payload[start] > 4 }
                or: { payload[start + 2].abs > 1.001 }
                or: { payload[start + 3].abs > 1.001 }
                or: { payload[start + 4] < 10 }
                or: {
                    payload[start + 4]
                        > (sampleRate * 0.5)
                }
                or: {
                    BiroReverbDesign.isExactInteger(
                        payload[start + 5], 0, 1
                    ).not
                }
                or: {
                    BiroReverbDesign.isExactInteger(
                        payload[start + 6], 0, 1
                    ).not
                }
                or: {
                    payload[start + 5]
                        != payload[start + 6]
                }) {
                Error(
                    "GeometryReverbScene: invalid path entry."
                ).throw
            }
        };
        if(payload[lateOffset] < 0
            or: { payload[lateOffset] > (pi / 2) }
            or: {
                BiroReverbDesign.validateFDNLines(
                    payload, fieldC, 1, lateOffset + 1
                ).not
            }) {
            Error(
                "GeometryReverbScene: invalid late field."
            ).throw
        };
        ^true
    }

    prepare { ^this }

    plot {
        ^size.plot("GeometryReverb shoebox dimensions")
    }
}
