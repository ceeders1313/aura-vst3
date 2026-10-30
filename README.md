# AURA v0.2 - moteur sample

## Build
GitHub : pousser le dossier, l'artefact `AURA-vst3-win64` est produit par Actions (build.yml fourni, inchange).

## Charger des samples
Bouton "Load samples..." ou glisser-deposer fichiers/dossier sur la fenetre. Formats : WAV, AIFF, FLAC.

## Convention de nommage (cartographie automatique)
    Piano_C3.wav            -> note racine C3 (C4 = MIDI 60)
    Piano_C3_v2.wav         -> couche de velocite n°2 pour C3
    Piano_C3_rr2.wav        -> variante round-robin n°2
    Piano_F#3_v1_rr1.wav    -> combinaisons possibles
- Les plages de touches s'etendent au milieu entre deux racines voisines.
- N couches de velocite -> la plage 1..127 est decoupee en N parts egales (v1 = la plus douce).
- Sans nom de note : 1 fichier = 60 sur tout le clavier ; plusieurs fichiers = chromatique des C2 (36) par ordre alphabetique.
- Boucles : lues depuis le chunk `smpl` des WAV (Loop0Start/Loop0End) si present.
- One-shot : ignore le note-off pour les samples sans boucle (batteries).

## Limites v0.2
- Samples charges entierement en RAM (plafond 1,5 Go). Le streaming disque arrive avec le format .aurapack.
