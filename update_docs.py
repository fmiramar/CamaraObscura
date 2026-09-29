import glob
import re

descriptions = {
    "DarkVelvetReverb": """Conceptually, a "velvet noise" reverb generates its tail not by recirculating audio through delays and filters, but by directly convolving the input with a highly sparse, randomized sequence of impulses (velvet noise). Because the pulses are extremely brief and randomly distributed, they create a dense, smooth wash of sound without the metallic ringing or repeating echoes that can plague feedback-based algorithmic reverbs.

Technically, `DarkVelvetReverb` implements the "dark velvet noise" extension. Instead of simply placing raw impulses, it substitutes filtered pulses from a predefined frequency-shaping dictionary. By varying the probability of selecting darker or brighter filters over the duration of the tail, it accurately models frequency-dependent air absorption and material damping, resulting in a naturally decaying, coloration-free reverberation that sounds fundamentally different from traditional FDNs or Schroeder reverbs.

""",
    "GeometryReverb": """Conceptually, a geometry-based reverb attempts to simulate the actual physical paths that sound takes when bouncing around a room, rather than just statistically approximating the decay. By calculating the exact timing, direction, and attenuation of the direct sound and early reflections bouncing off the walls, floor, and ceiling, it creates a much more convincing spatial impression of being in a real physical space.

Technically, `GeometryReverb` utilizes the image-source method for a shoebox-shaped room to generate up to six first-order early reflections. It interpolates these paths in real-time as the source position moves, preventing zipper noise. To avoid the extreme computational cost of modeling thousands of higher-order reflections, the early reflection engine seamlessly hands off the remaining energy to a shared Feedback Delay Network (FDN) late-field core, offering an efficient hybrid approach that marries spatial accuracy with computational efficiency.

""",
    "GroupedFDN": """Conceptually, a Grouped Feedback Delay Network (FDN) models acoustic spaces that consist of multiple distinct "zones" or coupled rooms (like a cathedral nave coupled to a transept). Instead of having one uniform cloud of reverberation, sound energy sloshes back and forth between these interconnected groups of delays, each of which can have entirely different decay characteristics.

Technically, `GroupedFDN` generalizes the standard FDN architecture by organizing the delay lines into separate sub-networks. The energy coupling between these groups is controlled by a symmetric matrix that is rigorously decomposed into pairwise Givens rotations. This guarantees that the feedback matrix remains perfectly orthogonal—and thus unconditionally stable—regardless of how the user modulates the group coupling or modifies the per-group decay filters at audio rates.

""",
    "ModalPlate": """Conceptually, modal synthesis creates reverberation by simulating the physical vibrations of a two-dimensional object—like a sheet of metal or glass—when struck. Unlike algorithmic reverbs that model air in a room, plate reverbs mimic the dense, shimmering, and instantly-dispersing mechanical waves of a physical plate, which has historically been highly prized in studios for thickening vocals and drums.

Technically, `ModalPlate` solves the Kirchhoff–Love plate equations for a rectangular boundary, breaking the physical vibration down into independent resonant modes. It then excites and reads from these modes using physical coordinates. By dynamically interpolating the physical mode shapes at block boundaries, the UGen allows both the excitation source and the pickup microphones to smoothly move across the surface of the plate in real-time, capturing realistic phase interactions and modal cancellations.

""",
    "ModalReverbBank": """Conceptually, a modal reverb bank constructs an acoustic space as a collection of thousands of independent ringing frequencies (modes). Just as a room has specific resonant frequencies that ring out when excited, this reverb uses a massive bank of resonators to build a tail. This allows for highly unnatural and creative spaces, such as rooms that only ring at harmonic intervals, or spaces whose resonances morph over time.

Technically, `ModalReverbBank` implements a dense parallel bank of complex, damped resonators. It can render up to 4096 individual modes simultaneously. Because each mode is completely independent, they can be subjected to global transformations such as spectral tilting, frequency-dependent dispersion, and precise decay-time scaling. This approach is fundamentally different from delay-based reverbs, allowing for completely resonant, non-echoic tails and the ability to cleanly "freeze" the reverb by suspending the damping coefficients.

""",
    "RIRFDN": """Conceptually, `RIRFDN` acts as a bridge between the realism of convolution reverbs and the flexibility of algorithmic reverbs. While traditional convolution exactly reproduces a recorded Room Impulse Response (RIR) but cannot be easily altered, this reverb extracts the statistical decay characteristics of a real room and synthesizes them using a flexible algorithmic structure, allowing you to tweak the "real" room in ways convolution cannot.

Technically, the system utilizes a deterministic offline Schroeder integration analysis to extract frequency-dependent decay rates (T60) from an impulse response. `RIRFDN` then uses these extracted parameters to tune a highly optimized Feedback Delay Network (FDN). This allows the UGen to capture the late-field spectral signature of a measured space with a fraction of the CPU and memory cost of full convolution, while providing the ability to freeze, modulate, or infinitely extend the tail.

""",
    "VelvetFDN": """Conceptually, `VelvetFDN` merges two powerful reverberation strategies: the smooth, echo-free density of velvet noise, and the infinite sustain of a Feedback Delay Network (FDN). Traditional FDNs can sometimes sound metallic or have audible repeating echoes before the echo density sufficiently builds up. By incorporating velvet noise, this reverb achieves a thick, lush tail instantly upon excitation.

Technically, `VelvetFDN` replaces the standard delay lines in an FDN with sparse, randomized velvet noise FIR filters. To maintain strict mathematical stability (losslessness) in the feedback loop when using non-ideal FIR filters, it employs an advanced paraunitary structure—a Hadamard–delay–Hadamard matrix formulation. This ensures the UGen remains unconditionally stable and artifact-free even with extreme decay times, offering a uniquely colored, rapidly-diffusing reverberation block.

"""
}

unified_related = "related:: Classes/DarkVelvetReverb, Classes/GeometryReverb, Classes/GroupedFDN, Classes/ModalPlate, Classes/ModalReverbBank, Classes/RIRFDN, Classes/VelvetFDN, Guides/CamaraObscura\n"

for f in glob.glob("HelpSource/Classes/*.schelp"):
    name = f.split('/')[-1].replace('.schelp', '')
    if name not in descriptions:
        continue
        
    with open(f, 'r') as file:
        content = file.read()
        
    # 1. Replace related:: with unified_related
    content = re.sub(r'related::.*?\n', unified_related, content, count=1)
    
    # 2. Add description paragraphs
    desc_start = content.find("description::\n")
    if desc_start != -1:
        insert_pos = desc_start + len("description::\n\n")
        content = content[:insert_pos] + descriptions[name] + content[insert_pos:]
        
    # 3. Remove "subsection:: See also" at the bottom
    content = re.sub(r'subsection:: See also.*', '', content, flags=re.DOTALL)
    
    # 4. Enhance the explanation before the first code block
    # Find examples::
    examples_idx = content.find("examples::\n")
    if examples_idx != -1:
        # Check if there's already an explanation
        if "The following unified test example" not in content[examples_idx:]:
            explanation = """
The following unified test example demonstrates the reverb using the provided high-quality dry CC0 audio samples.
By changing the `~sampleType` environment variable, you can test the reverberation characteristics on different types of sound sources (vocals, acoustic drums, guitars, or orchestral strings). If the `sounds/` directory is not found, it elegantly falls back to SuperCollider's built-in default sounds.

"""
            code_idx = content.find("code::", examples_idx)
            content = content[:code_idx] + explanation + content[code_idx:]
            
    with open(f, 'w') as file:
        file.write(content.strip() + "\n")
        
    print(f"Processed {name}")

