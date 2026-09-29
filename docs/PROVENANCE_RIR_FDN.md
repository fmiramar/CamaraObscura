# RIR-to-FDN provenance

Primary reference:

- Gloria Dal Santo, Benoit Alary, Karolina Prawda, Sebastian J. Schlecht,
  and Vesa Välimäki, “RIR2FDN: An Improved Room Impulse Response Analysis
  and Synthesis,” DAFx-24.
  <https://dafx.de/paper-archive/2024/papers/DAFx24_paper_20.pdf>

The work motivated strict separation between offline analysis and a compact
real-time FDN renderer. No upstream analyzer, optimizer, neural estimator,
code, impulse response, or fitted data was incorporated.

The bundled `rir2fdn` tool is original, dependency-free Python. It detects the
largest mono peak, selects a requested late start, computes deterministic
Schroeder energy regressions, estimates low/high decay through a one-pole
split, chooses seeded prime delays, writes a report, and renders verification
audio. The renderer uses the shared stable FDN core.

This first fit is deliberately broad: it is late-only, two-band, and does not
match early echoes, exact comb response, multichannel spatial covariance, or
the optimization quality of the referenced research system.

Implementation license: GPL-3.0-or-later. No measured RIR is distributed.
