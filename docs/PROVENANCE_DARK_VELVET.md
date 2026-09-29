# Dark Velvet provenance

Primary reference:

- Jon Fagerström, Sebastian J. Schlecht, and Vesa Välimäki,
  “Non-Exponential Reverberation Modeling Using Dark Velvet Noise,”
  *Journal of the Audio Engineering Society* 72(6), 2024.
  <https://arxiv.org/abs/2403.20090>

The publication was used for the sparse filtered-pulse model, evolving energy
envelopes, filter dictionaries, and grid-density normalization. No source
code, figure, table, supplementary audio, or model data was copied.

CamaraObscura changes and original work include the 12-float prepared-model
header, piecewise dB/probability payload, six short analytic dictionary
kernels, deterministic multichannel routing, hard event bound, and custom
uniform partitioned convolver. The runtime is feedforward and wet-only.

Known differences are explicit: no binaural coherence model, six rather than
an arbitrary learned dictionary, fixed 4096-sample partitions, profile/FFT
construction at Synth creation, and a maximum 30-second profile.

Implementation license: GPL-3.0-or-later.
