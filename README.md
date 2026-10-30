# AURA v0.4 - layers, routage, effets, arpegiateur, 2 MSEG

## Build
GitHub Actions (build.yml inchange). Artefact : AURA-vst3-win64.

## Layers (LayerParams.h : kLayers = 4, modifiable)
Chaque couche a son moteur complet : Sound (Saw/Square/Wavetable/FM + samples), Mod (8 slots,
2 LFO, enveloppe de mod, 4 macros), MSEG (2 courbes independantes), son propre dossier de samples.
Sources de mod : Mod Env, LFO 1/2, MSEG 1/2, Velocity, Key, Mod Wheel, Random, Macro 1-4.

## Routing
Par couche : On, plage de touches, plage de velocite, transposition, niveau, pan,
envois Chorus / Delay / Reverb, flag Arp.

## FX
Saturation (ADAA tanh / hard clip / asymetrique) -> bus Chorus, Delay (ping-pong),
Reverb FDN 8 lignes (Hadamard) -> Master -> Limiteur look-ahead 1,5 ms (latence rapportee au DAW).

## Arpegiateur (global, applique aux couches dont "Arp" est coche)
Up, Down, Up/Down, Random, As Played, Chord - 6 vitesses (dont triolets), 1-4 octaves,
gate, swing, latch. Synchro sur le tempo et la position du transport de l'hote.

## Tests suggeres
- Couche 1 : Saw unison, Routing : Reverb 0.4, Delay 0.2.
- Couche 2 : FM, Key Lo = 0 / Key Hi = 59 (split), Transpose -12.
- Arp On + Arp coche sur la couche 1 seulement : la couche 2 reste jouee en direct.
