# CamaraObscura

CamaraObscura is an experimental extension containing seven
architecturally distinct, wet-only reverberation systems for SuperCollider.
It combines clean-room
paper-derived techniques with original prepared-model formats and deliberately
creative controls rather than porting one upstream implementation.

The extension provides:

- `DarkVelvetReverb` creates arbitrary, non-exponential multichannel late
  tails from deterministic sparse filtered pulses. `DarkVelvetProfile`
  prepares evolving dB envelopes and dictionary probabilities, and the UGen
  renders them through a custom 4096-sample partitioned convolver.
- `GroupedFDN` models several independently damped energy reservoirs with
  runtime aperture control. `GroupedFDNModel` converts a symmetric coupling
  matrix into pairwise Givens angles, keeping every coupling value orthogonal
  before two-band attenuation.
- `VelvetFDN` accelerates echo-density growth using fixed sparse filters, with
  separate input/output and filtered-feedback modes. Its advanced mode uses a
  tested paraunitary Hadamard–delay–Hadamard matrix rather than assuming that
  normalized FIR entries are jointly lossless.
- `RIRFDN` renders an editable compact late field fitted from a room impulse
  response. The dependency-free `rir2fdn` tool performs deterministic
  two-band Schroeder analysis, writes a report/model, and renders verification
  audio; early reflections are deliberately excluded.
- `ModalReverbBank` synthesizes up to 4096 damped complex modes with pitch,
  time, dispersion, bounded excitation drive, and freeze. Models may be
  synthetic or created by the bundled transparent FFT-peak modal extractor.
- `ModalPlate` derives rectangular simply-supported Kirchhoff–Love modes from
  physical dimensions and material values. Excitation and pickup positions
  traverse smoothed physical mode shapes at block cadence.
- `GeometryReverb` interpolates a prepared moving-source shoebox scene with a
  direct path, six first-order image-source reflections, and the shared FDN
  late core. Meshes, diffraction, occlusion, elevation, and HRTFs are not
  claimed.

All complex state is described by validated, versioned mono Buffers. The
recursive renderers provide reset, the FDN and generic modal renderers provide
freeze, and no calculation callback allocates, performs I/O, parses a model,
factorizes a matrix, builds an FFT plan, locks, waits, or logs. All outputs are
wet only.

## Build

```sh
cmake -S . -B build \
  -DSC_PATH=/path/to/supercollider \
  -DSCSYNTH=ON -DSUPERNOVA=OFF \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
cmake --install build --prefix /path/to/SuperCollider/Extensions
```

Use a SuperCollider source tree matching the target server ABI. Build
supernova separately with `-DSCSYNTH=OFF -DSUPERNOVA=ON`.

For local verification, install into the SuperCollider user Extensions
directory and recompile the class library. Model sample rate must exactly
match the server that renders it.

## Offline tools

Analyze a measured integer-PCM WAV into a late-only model:

```sh
python3 tools/rir2fdn/rir2fdn.py analyze input.wav hall.rfdn \
  --late-start 0.08 --delays 8 --sample-rate 48000
```

Extract a compact modal approximation:

```sh
python3 tools/modal_extract/modal_extract.py analyze input.wav room.modal \
  --modes 512
```

Both tools are deterministic and have no third-party Python dependencies.

## Current scope

`GeometryReverbScene.shoebox` prepares first-order shoebox reflections on a
fixed interpolation grid plus a shared FDN late field. Arbitrary meshes are not
claimed. `RIRFDN` models late reverberation only. `HOADirectionalFDN` belongs
to a separate project specification. `ScatteringDelayReverb` is not
implemented or distributed pending legal review.

See the installed `CamaraObscura` guide, individual class help,
`docs/implementation-log.md`, and `benchmarks/RESULTS_2026-07-30.md` for
preparation, runnable examples, limitations, verification, and measurements.

## Audio Samples

The test sounds bundled in `sounds/` are provided under the Creative Commons CC0 1.0 Public Domain Dedication.
- `vocal.wav` is derived from the [Voice Samples Pack](https://freesound.org/people/honest_cactus/sounds/369929/) by *honest_cactus*.
- `drum1.wav` is a CC0 dry jazz breakbeat compiled from Archive.org.
- `drum2.wav` is derived from [Think Beat 126 BpM Loop - HipHop](https://freesound.org/people/lennartgreen/sounds/566905/) by *lennartgreen*.
- `guitar.wav` and `violin.wav` are CC0 test samples compiled from the ThinkDSP repository.

## License

CamaraObscura is licensed under GPL-3.0-or-later. Papers are cited as
algorithmic sources; no external DSP source, impulse response, preset, or
proprietary model data is incorporated.
