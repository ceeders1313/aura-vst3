# AURA v0.6 - nouvelle interface (page par couche, routage en graphe)

## Build
GitHub Actions (build.yml inchange). Artefact : AURA-vst3-win64.

## Nouveau en v0.6 (UI uniquement, moteur inchange)
- Barre du haut : logo, emplacement preset (browser a venir), pages LAYER / ROUTING / ARP, Output.
- Bandeau des couches : un onglet par couche (couleur, on/off, type de son).
- Page LAYER, sections toujours visibles : Oscillator/Sample | Filter (courbe) | Amp (enveloppe dessinee),
  Modulation sources (Mod Env, LFO 1/2, MSEG 1/2, Step Seq) | Mod matrix, Zone / Sends / Macros.
- Page ROUTING : graphe Couches -> FX (Chorus, Delay, Reverb) -> Master FX -> OUT ; epaisseur des cables
  = niveau / envoi ; clic sur un noeud = inspecteur a droite ; double-clic sur une couche = sa page.

## Nouveau en v0.5
- **Step sequenceur par couche** (onglet Layer n > Seq) : 16 pas, lignes Gate / Velocity / Pitch,
  Length 1-16, Rate (1/4 a 1/32, triolets), Gate, Swing. Les touches tenues sont rejouees a chaque pas
  actif, transposees de la hauteur du pas. Synchro sur le tempo / transport de l'hote.
  Chaine MIDI d'une couche : clavier -> (Arp si coche) -> Seq (si actif) -> synthe.
- **Correctifs** : fondu de 4 ms des voix volees (plus de clic), queue annoncee 15 s (reverbe non coupee
  au rendu), lissage des parametres (cutoff, resonance, WT, detune, FM, macros, niveaux, pan, envois,
  retours FX, master) pour supprimer le zipper.

## Layers (LayerParams.h : kLayers = 4, modifiable)
Chaque couche a son moteur complet : Sound (Saw/Square/Wavetable/FM + samples), Mod (8 slots,
2 LFO, enveloppe de mod, 4 macros), MSEG (2 courbes independantes), Seq, son propre dossier de samples.
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
- Couche 1 : Saw, Seq on, Rate 1/16, eteindre quelques pas, Pitch +12 / +7 sur certains : tenir un accord.
- Tourner Cutoff / Level rapidement : plus de zipper.
- Unison 7 + accords rapides (vol de voix) : plus de clics.
- Couche 2 : FM, Key Lo = 0 / Key Hi = 59 (split), Transpose -12.
