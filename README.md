# AURA v0.3 - synthese + modulation

## Build
GitHub Actions (build.yml fourni). Artefact : AURA-vst3-win64.

## Synthese (onglet Sound)
- Saw / Square (PolyBLEP) / Wavetable (8 frames, mipmaps 11 niveaux) / FM (3 operateurs).
- Unison 1..7 + detune (saw, square, wavetable). FM : 3 algorithmes, ratios, indice, feedback.
- Samples (v0.2) combinables : Source = Synth / Sample / Synth+Sample.

## Modulation (onglet Mod)
8 slots : Source -> Destination x Amount (-1..1), additifs.
Sources : Mod Env, LFO 1/2, MSEG, Velocity, Key, Mod Wheel (CC1), Random (par note), Macro 1-4.
Destinations : Pitch (+-24 st), Cutoff (+-5 oct), Resonance, WT Position, FM Index, Detune, Level.
Controle a 32 echantillons. Pitch bend +-2 st.

## MSEG (onglet MSEG)
Jusqu'a 16 points. Double-clic : ajouter/supprimer. Losange orange : courbure du segment.
Temps (0.01-20 s) + Loop. Sauvegarde dans le projet.

## Recettes de test
- Pad evolutif : Wave=Wavetable, mod slot 1 = LFO 1 -> WT Position 0.8 (LFO1 rate 0.3 Hz).
- Pluck : Mod Env -> Cutoff +0.8, Env Decay 0.25, Sustain 0.
- Pluck FM : Wave=FM, slot MSEG -> FM Index 0.6, MSEG decroissant.
- Vibrato : LFO 2 -> Pitch 0.02 (mod wheel : slot Mod Wheel -> Pitch 0.05).
