import numpy as np
import matplotlib.pyplot as plt
import os

out_dir = "HelpSource/Guides"
os.makedirs(out_dir, exist_ok=True)
sr = 48000
dur = 0.5
t = np.linspace(0, dur, int(dur * sr))

def save_plot(name, sig):
    plt.figure(figsize=(8, 2.5))
    plt.plot(t, sig, color='#4A90E2', linewidth=0.5, alpha=0.9)
    plt.axis('off')
    plt.tight_layout()
    plt.savefig(f"{out_dir}/{name}.png", transparent=True, dpi=150)
    plt.close()

np.random.seed(42)
dv = np.zeros_like(t)
pulse_indices = np.random.choice(len(t), size=150, replace=False)
dv[pulse_indices] = np.random.randn(150) * np.exp(-t[pulse_indices] * 8)
save_plot('ir_darkvelvet', dv)

geo = np.zeros_like(t)
er_idx = [int(x*sr) for x in [0.01, 0.025, 0.04, 0.06, 0.085, 0.12]]
for i, idx in enumerate(er_idx):
    geo[idx:idx+20] = np.random.randn(20) * (1 - i*0.15)
geo[int(0.1*sr):] += np.random.randn(len(geo) - int(0.1*sr)) * np.exp(-t[int(0.1*sr):] * 10) * 0.3
save_plot('ir_geometry', geo)

env_grp1 = np.exp(-t * 12)
env_grp2 = np.sin(t * np.pi * 3) * np.exp(-t * 5)
grp = np.random.randn(len(t)) * (env_grp1 + np.abs(env_grp2) * 0.5)
save_plot('ir_grouped', grp)

modal_plate = np.zeros_like(t)
for freq in [300, 450, 780, 1120, 1550, 2100, 3400, 4100]:
    modal_plate += np.sin(2 * np.pi * freq * t) * np.exp(-t * (freq/500))
save_plot('ir_plate', modal_plate)

modal_bank = np.zeros_like(t)
for freq in [150, 310, 625, 1250, 2500]:
    modal_bank += np.sin(2 * np.pi * freq * t + np.random.rand()*np.pi) * np.exp(-t * 6)
save_plot('ir_modalbank', modal_bank)

rir = np.random.randn(len(t)) * np.exp(-t * 8)
save_plot('ir_rirfdn', rir)

velvet = np.random.randn(len(t)) * np.exp(-t * 7)
velvet = np.clip(velvet, -0.4, 0.4) * np.exp(-t * 2)
save_plot('ir_velvetfdn', velvet)

print("Plots generated without titles")
