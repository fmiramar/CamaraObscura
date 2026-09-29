# CamaraObscura implementation log

Date: 2026-07-30

This log records the implementation and verification of the first
CamaraObscura release described by
`CREATIVE_REVERBERATORS_SUPERCOLLIDER_IMPLEMENTATION.md`. It is intentionally
more detailed than the project README so that design decisions, corrections,
tests, and limitations remain reviewable.

## Scope and classification

The work created the standalone `CamaraObscura` project. It does
not modify legacy reverbs or any ChowDSP repository.

The following public renderers and preparation classes were implemented:

- `DarkVelvetReverb` and `DarkVelvetProfile`
- `GroupedFDN` and `GroupedFDNModel`
- `VelvetFDN` and `VelvetFDNModel`
- `RIRFDN` and `RIRFDNModel`
- `ModalReverbBank` and `ModalReverbModel`
- `ModalPlate` and `ModalPlateModel`
- `GeometryReverb` and `GeometryReverbScene`

`HOADirectionalFDN` was not implemented because it has a separate
specification. `ScatteringDelayReverb` was not implemented because its legal
gate has not been cleared. The latter decision is recorded in
`docs/LEGAL_SCATTERING_DELAY_NETWORK.md`.

Before adding the classes, the requested names were searched in the workspace,
the installed SuperCollider class tree, the vanilla class library, installed
extensions, and available Quarks. No collision was found. The final class
library compiles with all fourteen names unchanged.

## Source and provenance decisions

The DSP is a clean-room implementation from the publications listed in
`ORIGINS.md` and the seven `docs/PROVENANCE_*.md` files. SuperCollider 3.14.1
source was reviewed for the server ABI, Buffer handling, UGen construction,
calculation-rate behavior, RT allocation, and plugin registration.

Vanilla and legacy reverb implementations were used only as API and lifecycle
references. No FreeVerb, GVerb, sc3-plugins reverb DSP, external source code,
paper figure, measured impulse response, factory preset, or commercial model
data was copied into the project.

The project target license is GPL-3.0-or-later. Source files carry matching
SPDX identifiers, `LICENSE` contains the complete GPLv3 text, and
`THIRD_PARTY_NOTICES.md` separates interface use, algorithmic references, and
distributed assets.

## Shared model ABI

All seven model families use a mono Float Buffer with an exact twelve-float
header:

```text
magic
version
headerSize
totalSize
sampleRate
channelCount
flags
seed
fieldA
fieldB
fieldC
fieldD
```

The family magics are `444001` through `444007`, and the first format version
is `1`. The language and server both reject unsupported versions, fractional
integer fields, impossible sizes, non-finite values, incompatible sample
rates, incorrect channel counts, and family-specific malformed payloads.

Family validation now covers segment continuity, Givens angles, line/group
order, FDN delays and T60 values, sparse taps, paraunitary scattering delays,
modal frequency and decay bounds, plate mode indices, geometry paths, and
late-field topology. The round-trip test also corrupts one family-defining
payload value in each of the seven formats and verifies rejection.

Prepared objects provide validation, summaries, text serialization, Buffer
upload, deterministic seed retention, diagnostics, and Buffer cleanup.
Model Buffers are immutable for a Synth's lifetime. Renderers silence
themselves if the Buffer identity or storage changes.

## Shared native infrastructure

The implementation added reusable fixed-storage C++ components for:

- integer and linearly interpolated fractional delay banks;
- Householder, normalized Hadamard, signed-permutation, dense-orthogonal, and
  Givens-cascade transforms;
- pairwise grouped Givens coupling;
- two-band attenuation filters fitted from per-line T60 targets;
- sparse fixed-tap velvet FIRs;
- a shared FDN core with bounded freeze and allocation-free reset;
- complex modal banks with structure-of-arrays state;
- rectangular plate equations and mode shapes;
- shoebox image-source path references;
- a uniform partitioned convolver and internal radix-2 FFT;
- deterministic PRNG, finite-input handling, denormal flushing, smoothing,
  reset edges, model readers, and guarded RT allocation.

No calculation callback allocates memory, resizes storage, performs file I/O,
parses a model, fits parameters, constructs an FFT plan, locks, waits, or
logs. Model parsing and fixed RT allocation happen during UGen construction;
offline analysis remains outside the audio server. Runtime coefficient
updates are bounded to block cadence where required.

All renderers return wet audio only. Recursive FDN and generic modal
renderers implement stable freeze by suppressing excitation and moving
attenuation toward a bounded radius/gain below unity. Reset clears state
without reallocating.

## Server object-lifetime correction

Live testing exposed an important SuperCollider plugin ABI detail: server
allocation initializes the `Unit` base fields but does not invoke a C++
constructor for the registered derived UGen type. Relying on C++ default
member initializers therefore left stale pointer values when the RT pool
reused graph memory; this was first visible when repeatedly constructing the
input/output VelvetFDN mode.

The shared `constructUnit` helper now:

1. saves the server-owned `Unit` base;
2. placement-constructs the complete derived object;
3. restores the server-owned base fields.

Every destructor releases its owned RT allocations and explicitly ends the
derived object lifetime. This correction applies consistently to all seven
UGens and was retested in both scsynth and supernova.

## Renderer implementations

### DarkVelvetReverb

`DarkVelvetProfile` stores contiguous piecewise dB segments and six evolving
dictionary probabilities. The renderer deterministically generates signed,
decorrelated sparse filtered tails from low-pass, high-pass, band-pass,
shelf-like, and broad-peak kernels.

The generated response is rendered by a custom 4096-sample uniform
partitioned convolver. It supports one through eight outputs, a maximum
thirty-second profile, a bounded 250,000 events per output, deterministic
seeds, arbitrary swelling/gating/reverse-like envelopes, and allocation-free
reset.

The explicit tradeoff is 4096 samples of latency. Spectrum memory and FFT
multiply work grow linearly with output count and tail partition count.

### GroupedFDN

Each group has its own delay range and low/high T60 pair. Intra-group
Householder mixing is followed by one prepared Givens rotation for every
group pair. Runtime `coupling` scales every angle, so the lossless prototype
remains orthogonal from isolated groups through the prepared aperture.
Contractive two-band line attenuation is applied separately.

The model requires a finite symmetric coupling matrix with unit diagonal.
Frequency-dependent loss is present per group, while the pairwise coupling
angle itself is full-band in this release.

### VelvetFDN

Mode zero places deterministic sparse FIRs around a shared FDN. Mode one adds
an explicit paraunitary feedback structure:

```text
H D(z) H
```

where `H` is a normalized Hadamard transform and `D(z)` is a diagonal bank of
pure delays. This construction is lossless before attenuation; it does not
infer losslessness from independently normalized FIR entries.

The test evaluates the complete complex matrix on a 1024-point frequency
grid and measures the deviation of its Gram matrix from identity.

### RIRFDN

`RIRFDN` is deliberately a late-field model, not a complete room-impulse
response player. It uses the shared FDN with fitted low/high T60 values,
decay scaling, tone control, freeze, reset, and safe morphing by running two
complete compatible stable networks and crossfading their outputs.

The dependency-free `tools/rir2fdn` analyzer reads integer-PCM WAV, detects
the direct peak, selects the requested late start, performs broadband and
two-band Schroeder decay fitting, chooses deterministic prime delays, writes
the prepared model and a human-readable report, and renders a verification
WAV.

### ModalReverbBank

The modal renderer uses damped complex one-poles with contiguous
structure-of-arrays state. Mode frequency, amplitude T60, input residue,
stereo output residues, and phase are prepared. Phase sine/cosine values are
cached, and pole coefficients update once per block for pitch, decay,
damping tilt, dispersion, and freeze.

Drive is a bounded excitation nonlinearity; no uncontrolled nonlinearity is
placed inside modal feedback. Freeze suppresses new excitation and moves
radii toward `0.9999995`.

Models may be explicit, deterministic random, harmonic, or extracted with the
bundled Hann-window FFT peak tool. The extractor is documented as a compact
transparent approximation, not as ESPRIT or matrix-pencil analysis.

### ModalPlate

The preparation class derives simply-supported rectangular
Kirchhoff-Love modes from width, height, thickness, density, Young's modulus,
Poisson ratio, and damping. The implementation stores integer mode pairs,
physical frequencies, modal T60, and normalization.

Excitation and pickup coordinates evaluate the physical sin-sin mode shapes
at block cadence and are smoothed over approximately 20 ms. Expensive
per-mode trigonometry is not performed per audio sample. Output scaling was
made mode-count-aware to retain headroom for large banks.

Only the simply-supported boundary is claimed. Drive and pitch remain
artistic controls, not claims of nonlinear physical plate tension.

### GeometryReverb

The scene builder prepares a two-through-four point grid on every room axis.
Each source/listener grid pair stores one direct and six first-order
image-source paths with delay, inverse-distance gain, horizontal direction,
material-derived cutoff, type, and generation. A shared eight-line FDN
provides the late field.

At runtime the UGen performs six-dimensional corner interpolation once per
block, then linearly ramps path parameters. Fractional delays are read before
the current sample is written so their requested delay is not one sample
short. One-pole filter poles are calculated once per path per block and
ramped, replacing an earlier per-path, per-sample exponential calculation.

Positions are clipped to the room. Only yaw is used; pitch and roll are
reserved. Meshes, visibility, diffraction, occlusion, higher-order
reflections, elevation, HRTFs, and HOA output are not claimed.

## Correctness findings during hardening

The following issues were found by targeted tests and fixed before the final
verification:

- C++ default member initializers were not being invoked by the server UGen
  allocator; explicit derived-object lifetime management fixed recycled RT
  pool pointers.
- Grouped coupling originally collapsed a complete coupling matrix to one
  aperture. The format and renderer now retain one Givens angle per pair.
- Grouped model preparation initially collapsed requested group delay ranges
  to a global range. Each group now receives a deterministic independent
  distribution inside its own range.
- SuperCollider evaluates binary operators left-to-right. Explicit
  parentheses corrected the delay interpolation expression and the
  Kirchhoff-Love spatial-frequency sum. Language tests now compare both
  results directly against their intended ranges/equation.
- Geometry's fractional delay order was one sample early; the renderer now
  reads before writing.
- Geometry initially calculated a one-pole exponential for every path at
  every sample. It now computes target poles once per block and ramps them.
- Modal phase trigonometry was removed from the per-mode sample loop.
- Plate and geometry outputs received fixed, documented path/mode-count
  normalization after live peak measurements.
- The DarkVelvet maximum event bound was raised to cover the complete
  documented thirty-second, 8000-events/second range.
- Modal constructors gained finite-value, Nyquist, T60, gain-shape, count,
  and sample-rate checks.

## Offline tools

The project includes:

- `tools/rir2fdn/rir2fdn.py`
- `tools/modal_extract/modal_extract.py`
- `tools/model_inspect/model_inspect.py`
- shared dependency-free WAV/model/FFT utilities in `tools/model_common.py`

The tools accept integer-PCM WAV and do not require NumPy, neural models, or
network downloads. Procedural fixtures verify deterministic output, decay
relationships, modal peak recovery near 311 Hz and 3023 Hz, model inspection,
report generation, and verification rendering. Python bytecode is disabled
and excluded from installed/release artifacts.

## Documentation and examples

Fourteen exact class help files and one project guide were created under the
required SCDoc locations. Each class page documents origin, architecture,
limits, controls, channel behavior, reset/freeze semantics, CPU or memory
implications, model preparation, a vanilla comparison where useful, and
simple See-also links.

Eight standalone examples cover:

- gated and reverse-like DarkVelvet tails;
- moving grouped-room coupling;
- dense paraunitary VelvetFDN;
- RIRFDN room morphing;
- a harmonic modal room;
- moving plate pickup;
- moving shoebox geometry.

Examples that use audio default to a looping `ExampleFiles.child` PlayBuf and
include a commented `SoundIn` alternative. Buffer setup runs in a Routine,
waits with `s.sync`, and applies a short fade-in.

`SCDoc.indexAllDocuments(true)` indexed 2872 documents. All fourteen class
pages and `Guides/CamaraObscura` then parsed and rendered without an SCDoc
warning or error.

## Build and packaging

CMake supports separate scsynth and supernova targets, strict warnings,
native reference tests, optional sanitizers, optional benchmarks, staged
installation, and exclusion of generated Python caches.

The CI workflow builds clean checkouts for Linux, macOS x64/arm64, and Windows
x64, includes a Linux supernova job and sanitizer job, stages an installable
extension, and names artifacts with the GitHub repository owner plus project,
platform, and architecture.

The final local installation contains both `CamaraObscura.scx` and
`CamaraObscura_supernova.scx`, classes, SCDoc, examples, tools, provenance,
and benchmark records.

## Verification record

### Native strict and sanitizer tests

The strict Release scsynth build and the strict supernova build both completed
with the configured warning set. In both trees:

```text
creative_reverbs_reference_tests  Passed
creative_reverbs_offline_tools    Passed
100% tests passed
```

The ASan/UBSan reference build also passed both tests with no sanitizer
diagnostic.

Native checks cover:

- exact model headers and invalid sizes;
- integer/fractional delay timing and clearing;
- Householder, Hadamard, signed permutation, dense orthogonal, and all
  pairwise Givens energy preservation;
- bounded attenuation and ten-second FDN/freeze renders;
- sparse velvet timing;
- 1024-frequency paraunitary identity;
- exact partitioned-convolution values and latency;
- analytical modal radius, finite long renders, stereo residues, and freeze;
- DarkVelvet envelope/dictionary behavior;
- plate frequency and nodal cancellation;
- shoebox first-order delay and inverse-distance gain.

### SuperCollider language and server tests

The following scripts passed:

```text
compile_check.scd
model_roundtrip.scd
example_compile.scd
invalid_buffer_smoke.scd
live_smoke.scd
nrt_sample_rate_matrix.scd
supernova_probe.scd
verify_scdoc.scd
```

The model test round-tripped every family, verified all family validators
reject corrupt payloads, and exercised sclang-to-Python modal extraction.

The final combined scsynth live smoke rendered fifteen channels covering
DarkVelvet, GroupedFDN, both Velvet modes, RIR morphing, ModalReverbBank,
ModalPlate, and GeometryReverb:

```text
[0.6158, 0.6312, 0.1214, 0.1330, 0.0195,
 0.0157, 0.0443, 0.0464, 0.0602, 0.0604,
 0.1164, 0.1146, 0.2552, 0.1060, 0.1336]
```

All values are per-channel absolute peaks. Every architecture was non-silent
and no sample was NaN or infinite.

The NRT matrix rendered all seven architectures at 44.1, 48, and 96 kHz. It
reported finite, nonzero peaks and completed with
`CREATIVE_REVERBS_NRT_MATRIX_OK`. The freshly installed supernova binary
separately constructed and freed every family and completed with
`CREATIVE_REVERBS_FAMILY_PROBE_OK`.

Invalid model Buffers caused all seven UGens to fail safely while leaving the
server responsive.

## Benchmark record

The benchmark used a Release C++17 native x86_64 executable, Apple Clang 17,
float sample state, and a nominal 48 kHz conversion rate. Full machine and
method conditions are in `benchmarks/RESULTS_2026-07-30.md`.

| Kernel | Size | ns/sample | One core at 48 kHz | Counted state |
|---|---:|---:|---:|---:|
| FDNCore | 8 lines | 48.68 | 0.234% | 36,544 B |
| FDNCore | 16 lines | 91.40 | 0.439% | 82,816 B |
| FDNCore | 32 lines | 171.44 | 0.823% | 204,544 B |
| ModalBank | 64 modes | 197.49 | 0.948% | 3,072 B |
| ModalBank | 256 modes | 762.62 | 3.661% | 12,288 B |
| ModalBank | 1,024 modes | 3,093.28 | 14.848% | 49,152 B |
| ModalBank | 2,048 modes | 6,358.71 | 30.522% | 98,304 B |
| PartitionedConvolver | 2 ch × 6 partitions | 131.97 | 0.633% | 802,960 B |
| PartitionedConvolver | 8 ch × 6 partitions | 396.39 | 1.903% | 2,179,504 B |

The data show approximately linear scaling for FDN lines and modal count.
They also show why a 4096-mode maximum is design headroom rather than a claim
that the maximum is inexpensive. DarkVelvet convolution is CPU-efficient for
the measured partition count, but spectrum memory dominates and latency is
fixed at 4096 samples.

These are kernel costs, not reverb-quality rankings.

## Deliberate first-release limitations

- DarkVelvet uses a fixed six-filter dictionary and a 4096-sample
  partitioned-convolution latency.
- Grouped coupling is a full-band orthogonal Givens morph; frequency-dependent
  group loss is implemented, but frequency-dependent inter-group transmission
  remains a later extension.
- `VelvetFDNModel.filteredMatrix` uses the supplied velvet-filter collection
  to select tap count; taps themselves are deterministic in version 1.
- RIRFDN is late-only and uses transparent two-band fitting rather than an
  exact reconstruction or a neural estimator.
- The modal extractor is FFT-peak based and cannot identify every strongly
  overlapping measured mode.
- The modal loop is contiguous and SIMD-friendly but uses portable scalar C++
  rather than platform-specific intrinsics.
- ModalPlate supports simply-supported rectangular plates only.
- GeometryReverb supports a shoebox, direct plus first-order paths, stereo
  horizontal panning, and yaw only. Mesh and HOA support are not advertised.
- Prepared model sample rate must match the server.
- ScatteringDelayReverb remains legally blocked.

These limitations are documented in the public help and are not hidden behind
unimplemented controls or unsupported claims.
