# AURA - VSTi (JUCE 8 / C++20) - Windows x64

## Compiler (local)
Installer Visual Studio 2022 (C++ desktop), CMake, Git, puis : `build_windows.bat`
Resultat : `dist\AURA.dll` et le bundle `build\AURA_artefacts\Release\VST3\AURA.vst3`.
Copier le dossier AURA.vst3 dans `C:\Program Files\Common Files\VST3\`.

## Compiler sans rien installer
Pousser ce dossier sur GitHub : l'onglet Actions produit l'artefact `AURA-vst3-win64`.

## Contenu v0.1
Synthe polyphonique 64 voix : Saw / Square / Supersaw (PolyBLEP), filtre SVF TPT,
ADSR, vol de voix, 9 parametres automatisables, etat sauvegarde.
