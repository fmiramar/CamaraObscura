import numpy as np
import matplotlib.pyplot as plt
import matplotlib.font_manager as fm

# Generate textbook IR data for velvet reverb
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
sig[0] = 0.8  # Direct signal
sig[int(0.005*sr)] = -0.5
sig[int(0.012*sr)] = 0.4
sig[tail_start:] = velvet

# Plot
fig, ax = plt.subplots(figsize=(12, 4), facecolor='#F8F8F8')
ax.set_facecolor('#F8F8F8')

# Subsample for discrete plotting so it looks like stems and doesn't overwhelm the plot
# For the dense tail, we plot standard lines but maybe thin black.
# Actually, the user asked for \dstems (discrete stems).
# Let's plot stems for the whole thing, but heavily subsampled for visual clarity
sub_idx = np.arange(0, len(t), int(sr/1000)) # plot 1000 stems per second

markerline, stemlines, baseline = ax.stem(t[sub_idx], sig[sub_idx], linefmt='#222222', basefmt=" ", markerfmt='k.')
plt.setp(markerline, markersize=3)
plt.setp(stemlines, linewidth=0.5)

# Add baseline
ax.axhline(0, color='#888888', linewidth=0.5)

# Remove axes
ax.axis('off')

# Try to find a medieval font
try:
    prop = fm.FontProperties(family='Luminari', size=55)
    ax.text(0.5, 0.8, 'Camara Obscura', color='#111111', fontproperties=prop, ha='center', va='center', alpha=0.85, transform=ax.transAxes)
except:
    ax.text(0.5, 0.8, 'Camara Obscura', color='#111111', fontsize=55, fontname='Times New Roman', style='italic', ha='center', va='center', alpha=0.85, transform=ax.transAxes)

plt.tight_layout()
plt.savefig('HelpSource/Guides/camara_obscura_banner.png', dpi=200, bbox_inches='tight', facecolor='#F8F8F8')
print("Minimalist \dstems banner generated successfully.")
