import re
import os

images = {
    'DarkVelvetReverb': ('ir_darkvelvet.png', 'DarkVelvetReverb shows randomized, sparse discrete pulses with a slowly decaying exponential envelope, mimicking the convolution of an audio signal against a sparse velvet noise train.'),
    'GeometryReverb': ('ir_geometry.png', 'GeometryReverb shows distinct high-amplitude early reflections preceding the diffuse late tail. This plot highlights the temporal gap between the direct geometric paths and the subsequent FDN wash.'),
    'GroupedFDN': ('ir_grouped.png', 'GroupedFDN shows the energy sloshing between sub-networks, creating a complex non-linear decay envelope where late energy swells as it transfers between the coupled modeled rooms.'),
    'ModalPlate': ('ir_plate.png', 'ModalPlate shows the high-frequency sinusoidal beating characteristic of a ringing physical structure. You can clearly see the interference of specific plate eigenmodes rather than broadband noise.'),
    'ModalReverbBank': ('ir_modalbank.png', 'ModalReverbBank shows a sparse mix of independent beating resonances. Each decay path is an entirely independent ringing modal filter.'),
    'RIRFDN': ('ir_rirfdn.png', 'RIRFDN shows an instantly smooth and dense exponential decay modeled directly from the Schroeder integration of a real space impulse response.'),
    'VelvetFDN': ('ir_velvetfdn.png', 'VelvetFDN shows an immediate, thick block of solid density that bypasses the slow build-up of traditional FDNs by embedding velvet sequences directly into the feedback loops.')
}

base_dir = "HelpSource/Classes"

for ugen, (img_file, caption) in images.items():
    file_path = os.path.join(base_dir, f"{ugen}.schelp")
    if not os.path.exists(file_path):
        continue
    
    with open(file_path, 'r') as f:
        content = f.read()
    
    # We want to insert the image right after the description:: tag (or after the first few paragraphs of description).
    # The safest is right after "description::\n\n"
    
    img_tag = f"image::../Guides/{img_file}#{caption}::\n\n"
    
    # Check if we already inserted an image tag
    if f"image::../Guides/{img_file}" in content:
        continue
        
    content = content.replace("description::\n\n", f"description::\n\n{img_tag}")
    
    with open(file_path, 'w') as f:
        f.write(content)

print("Images injected into Classes docs")
