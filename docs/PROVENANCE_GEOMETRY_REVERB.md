# Geometry reverb provenance

Technique references:

- Allen and Berkley’s standard image-source method for rectangular rooms.
- General artificial-reverberation and FDN literature cited in the project
  specification.

No geometry engine, RoomAcoustiC++ source, mesh loader, SDN implementation,
scene, material database, HRTF, or measured room data was copied.

`GeometryReverbScene.shoebox` is an original builder. It stores a direct path
and six first-order image-source reflections for every pair on a fixed 2–4
point-per-axis interpolation grid, followed by a model for the shared project
FDN. Each path includes delay, inverse-distance/reflection gain, horizontal
direction, low-pass cutoff, type, and generation.

The renderer interpolates the six source/listener dimensions at block cadence
and ramps parameters per sample. It does not implement arbitrary meshes,
visibility, diffraction, occlusion, elevation encoding, higher-order
reflections, HRTFs, or path-list updates from another thread.

The separate scattering-delay-network class remains blocked; see
`LEGAL_SCATTERING_DELAY_NETWORK.md`.

Implementation license: GPL-3.0-or-later.
