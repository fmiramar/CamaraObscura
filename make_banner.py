import numpy as np
import matplotlib.pyplot as plt
import matplotlib.font_manager as fm

sr = 48000
dur = 0.5
t = np.linspace(0, dur, int(sr * dur))

# Velvet noise tail (dense)
tail_start = int(0.02 * sr)
tail_len = len(t) - tail_start
tail_t = t[tail_start:]
velvet = np.random.randn(tail_len) * np.exp(-(tail_t - 0.02) * 8)
velvet = np.clip(velvet, -0.2, 0.2) * np.exp(-(tail_t - 0.02) * 4)

# Combine
sig = np.zeros_like(t)
sig[0] = 0.8
sig[int(0.005*sr)] = -0.5
sig[int(0.012*sr)] = 0.4
sig[tail_start:] = velvet

fig, ax = plt.subplots(figsize=(12, 4), facecolor='#F8F8F8')
ax.set_facecolor('#F8F8F8')

# Subsample heavily so we see individual connected triangles instead of a solid black block
# But for plotting with linear interpolation where 1 sample is non-zero, if we subsample, we might skip the non-zero sample entirely!
# Wait, if we use a mask of sparse peaks...
sparse_sig = np.zeros_like(sig)
# Place early reflections explicitly
sparse_sig[0] = 0.8
sparse_sig[int(0.005*sr)] = -0.5
sparse_sig[int(0.012*sr)] = 0.4

# Place sparse velvet pulses
num_pulses = 150
pulse_indices = np.random.randint(tail_start, len(t), size=num_pulses)
for idx in pulse_indices:
    decay = np.exp(-(t[idx] - 0.02) * 6)
    sparse_sig[idx] = (np.random.rand() * 0.4 - 0.2) * decay

# Plot with linear interpolation. Since the signal is mostly 0 with 1-sample spikes, it forms triangles!
ax.plot(t, sparse_sig, color='#222222', linewidth=0.8)

# Add grid and subgrid
ax.minorticks_on()
ax.grid(True, which='major', color='#cccccc', linewidth=0.8)
ax.grid(True, which='minor', color='#e0e0e0', linewidth=0.4, linestyle=':')

# Remove axis spines but keep ticks/labels? Or remove all?
# "Insert a grid and a subgrid"
ax.set_xticklabels([])
ax.set_yticklabels([])
ax.tick_params(axis='both', which='both', length=0)

# Typo
try:
    prop = fm.FontProperties(family='Luminari', size=55)
    ax.text(0.5, 0.8, 'Camara Obscura', color='#111111', fontproperties=prop, ha='center', va='center', alpha=0.85, transform=ax.transAxes, bbox=dict(facecolor='#F8F8F8', edgecolor='none', alpha=0.7, pad=10))
except:
    ax.text(0.5, 0.8, 'Camara Obscura', color='#111111', fontsize=55, fontname='Times New Roman', style='italic', ha='center', va='center', alpha=0.85, transform=ax.transAxes, bbox=dict(facecolor='#F8F8F8', edgecolor='none', alpha=0.7, pad=10))

plt.tight_layout()
plt.savefig('HelpSource/Guides/camara_obscura_banner.png', dpi=200, bbox_inches='tight', facecolor='#F8F8F8')
print("Banner generated successfully.")
