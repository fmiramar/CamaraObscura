# Origins

CamaraObscura is a clean-room implementation from mathematical descriptions
in the papers recorded in the project provenance documents.

- DarkVelvet follows the extended dark-velvet-noise architecture of
  Fagerström, Schlecht, and Välimäki, using original dictionary kernels,
  profile serialization, deterministic routing, and partitioned convolution.
- GroupedFDN follows the grouped-delay-network concept of Das, Abel, and
  Canfield-Dafilou. Runtime coupling scales prepared Givens angles, preserving
  orthogonality at every setting.
- VelvetFDN follows the input/output velvet-filter architecture of Fagerström
  et al. Its optional feedback mode is an original explicitly paraunitary
  Hadamard–delay–Hadamard construction.
- RIRFDN is informed by the deterministic analysis/render separation in
  RIR2FDN. The included analyzer and compact model format are original.
- ModalReverbBank follows the parallel complex-mode architecture described by
  Abel and Werner.
- ModalPlate derives simply-supported rectangular plate modes from the
  Kirchhoff–Love equation. Runtime position control evaluates those physical
  mode shapes at bounded control cadence.
- GeometryReverb uses the standard image-source equation for prepared
  first-order shoebox paths and the project’s shared FDN core for the late
  field.

SuperCollider 3.14.1 source was reviewed for current plugin ABI, Buffer
lifetime, calculation-rate, and allocation conventions. Its legacy FreeVerb
and GVerb DSP was not used.
