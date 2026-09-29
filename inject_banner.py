with open("HelpSource/Guides/CamaraObscura.schelp", "r") as f:
    content = f.read()
if "camara_obscura_banner.png" not in content:
    content = content.replace("title:: CamaraObscura", "title:: CamaraObscura\nsummary:: Advanced algorithmic reverberation models\n\nimage::camara_obscura_banner.png::\n")
    with open("HelpSource/Guides/CamaraObscura.schelp", "w") as f:
        f.write(content)
