with open("HelpSource/Guides/CamaraObscura.schelp", "r") as f:
    lines = f.readlines()

new_lines = []
skip = False
for line in lines:
    if line.startswith("summary:: Advanced algorithmic reverberation models"):
        continue
    if line.startswith("image::camara_obscura_banner.png::"):
        continue
    if line.startswith("description::"):
        new_lines.append(line)
        new_lines.append("\nimage::camara_obscura_banner.png::\n\n")
        continue
    new_lines.append(line)

# Also remove any duplicate empty lines around top
content = "".join(new_lines).replace("title:: CamaraObscura\n\n\nsummary::", "title:: CamaraObscura\nsummary::")

with open("HelpSource/Guides/CamaraObscura.schelp", "w") as f:
    f.write(content)
