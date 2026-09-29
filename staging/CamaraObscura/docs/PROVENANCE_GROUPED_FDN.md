# Grouped FDN provenance

Primary references:

- Orchisama Das, Jonathan Abel, and Elliot K. Canfield-Dafilou,
  “Delay Network Architectures for Room and Coupled Space Modeling,”
  DAFx-20.
  <https://dafx2020.mdw.ac.at/proceedings/papers/DAFx2020_paper_25.pdf>
- Orchisama Das, Sebastian J. Schlecht, and Enzo De Sena,
  “Grouped Feedback Delay Networks with Frequency-Dependent Coupling,”
  *IEEE/ACM TASLP*, 2023.

The publications supplied the high-level grouped-energy-reservoir and coupled
space concepts. No external implementation or prepared model was copied.

This implementation uses an original compact topology: one Householder
reflection per equal-sized group followed by a Givens rotation for every
group pair. Runtime coupling scales the prepared angles, which preserves
orthogonality exactly before per-line contractive attenuation. Each group
receives independent two-band T60 targets.

The first release does not implement coupling filters themselves. It models
frequency-dependent group losses, while group-to-group Givens exchange is
full-band. Arbitrary asymmetric matrices are rejected because the current
lossless parameterization requires a symmetric perceptual coupling request.

Implementation license: GPL-3.0-or-later.
