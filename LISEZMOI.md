# TaLaVie Guitare — plugin VST3 (guitare électrique afrobeat modélisée)

## Compiler sur Windows
1. Installer Visual Studio 2022 Community (cocher « Développement Desktop en C++ ») et CMake.
2. Ouvrir « x64 Native Tools Command Prompt for VS 2022 », aller dans ce dossier :
       cmake -B build -G "Visual Studio 17 2022"
       cmake --build build --config Release
   (le premier lancement télécharge JUCE, ça prend quelques minutes)
3. Le plugin est dans :
   build\TaLaVieGuitare_artefacts\Release\VST3\TaLaVie Guitare.vst3
4. Copier ce dossier .vst3 dans C:\Program Files\Common Files\VST3
5. FL Studio : Options > Gestionnaire de fichiers > Rechercher plugins, puis le trouver dans la liste.

Un exécutable de test est aussi créé : Standalone\TaLaVie Guitare.exe (utile pour tester sans FL Studio).

## Deux guitares dans le même plugin
Le réglage « Guitare » choisit : Afrobeat électrique (médiator, brillant) ou Douce chaleureuse (doigts, micro manche, ampli arrondi, plus de reverb).

## Réglages
Sustain, Release, Brillance, Chaleur ampli, Chorus, Delay (mix + temps en ms), Reverb (taille + mix), Volume.
La pédale de sustain MIDI fonctionne.
