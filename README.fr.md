# v32opt — Optimiseur d'Assembleur pour Vircon32

[Version anglaise / in English](README.md) | [Version espagnole / en español](README.es.md)

`v32opt` est un optimiseur d'assembleur modulaire à passes multiples,
écrit en C, qui cible spécifiquement la console de fantaisie
**Vircon32**. Il prend l'assembleur brut produit par un compilateur pour
Vircon32 (C ; C++ via `v32c++` ; et Lua via `v32lua`), de l'assembleur
écrit à la main ou (avec prudence, voir plus bas) des CARTs Vircon32
désassemblées, et applique des optimisations itératives locales,
structurelles, de flot de données et de promotion en registres pour
réduire la taille du code, voir à l'œuvre chacune des étapes de la chaîne
de construction et améliorer l'efficacité d'exécution.

> **Remarque sur l'architecture Vircon32 :** Sur le processeur Vircon32,
> **toutes les instructions s'exécutent en exactement 1 cycle**, et il
> n'existe **aucun indicateur (flag) du processeur réagissant aux
> instructions** (comme Carry, Zero ou Overflow). Les instructions de
> comparaison destructives (p. ex. `IEQ`, `INE`) écrasent directement le
> registre de destination avec `1` (vrai) ou `0` (faux), que les sauts
> conditionnels (`JT`, `JF`) évaluent directement. Bien que de nombreuses
> simplifications algébriques soient neutres en cycles du fait de ce
> modèle temporel uniforme, `v32opt` les applique de manière agressive
> pour réduire la taille totale du binaire, diminuer la pression sur les
> registres, éliminer les goulets d'étranglement mémoire et imposer un
> assembleur propre et idiomatique.


## Collaboration avec l'IA

Cet outil a été développé en étroite collaboration avec l'IA. J'hésite à
le qualifier de « vibe-codé », car j'ai quelques connaissances et une
certaine expérience des bases (de l'assembleur, de l'optimisation de
code). Mais je n'aurais certainement pas atteint ce niveau de
fonctionnalité en si peu de temps si l'IA n'avait pas été activement
impliquée. J'ai le sentiment d'avoir plutôt tenu un rôle de « chef de
projet » : fixer la direction, résoudre les problèmes, déboguer là où
c'était nécessaire.

Même si je ne connais peut-être pas chaque recoin du code aussi bien que
si je l'avais entièrement écrit moi-même, j'apprécie le levier qu'elle
offre pour créer de nouveaux outils que je peux utiliser en classe,
partager avec la communauté, et concrétiser des idées plus rapidement.

Je sais que cela peut frôler ou franchir, pour certains, une ligne d'une
ambiguïté ou d'une acceptabilité douteuse. Mais, s'agissant d'un outil de
niche dans un environnement de niche, puisse-t-il être tout aussi facile
à ignorer.

---

## Table des Matières

1. [Instructions de Compilation](#instructions-de-compilation)
2. [Intégration dans le processus de construction d'une CART](#intégration-dans-le-processus-de-construction-dune-cart)
3. [Langages Sources : C, C++, Lua et Assembleur](#langages-sources--c-c-lua-et-assembleur)
4. [Utilisation et Niveaux d'Optimisation](#utilisation-et-niveaux-doptimisation)
5. [Phase 1 : Optimisations Locales à Lucarne (-O1)](#phase-1--optimisations-locales-à-lucarne--o1)
6. [Phase 2 : Flot de Données Global et Code Mort (-O2)](#phase-2--flot-de-données-global-et-code-mort--o2)
7. [Phase 3 : Intégration et Optimisation des Boucles (-O3)](#phase-3--intégration-et-optimisation-des-boucles--o3)
8. [Promotion de la Mémoire en Registres (-O2)](#promotion-de-la-mémoire-en-registres--o2)
9. [Visualisation du CFG](#visualisation-du-cfg)
10. [Garde-fous de Sûreté et de Correction](#garde-fous-de-sûreté-et-de-correction)
11. [Options de Diagnostic et de Débogage](#options-de-diagnostic-et-de-débogage)
12. [Publication (estampillage de la version)](#publication-estampillage-de-la-version)

Voir aussi **[USAGE.md](USAGE.md)** (en anglais) : un guide en langage simple pour
choisir un niveau, vérifier le résultat, régler, profiler et mesurer.

---

## Instructions de Compilation

`v32opt` est écrit en C standard et conçu pour se compiler facilement sur
n'importe quelle plateforme avec un compilateur C moderne (`gcc` ou
`clang`). Les en-têtes du projet se trouvent dans `inc/` (séparés par
fonction : `v32opt.h` est l'en-tête général, avec `asm.h`, `peephole.h`,
`dataflow.h`, `inline.h`, `stack.h` et `promote.h` derrière lui) ; les
sources se trouvent dans `src/` et `src/peephole/`.

Il existe deux façons indépendantes de le compiler : le `Makefile` de
base (la compilation de développement habituelle) et CMake (une
compilation hors de l'arborescence des sources, avec prise en charge de
l'installation système et de l'empaquetage, inspirée de la configuration
CMake des DevTools de Vircon32). Elles ne partagent jamais leurs produits
de compilation, et peuvent donc être utilisées côte à côte.

### Avec Make (Linux / macOS / MSYS2)

```bash
# Compile le binaire (écrit dans bin/v32opt ; bin/v32opt.exe sous Windows)
$ make

# Installe dans votre répertoire bin personnel (~/bin/bin.<arch> s'il
# existe, sinon ~/bin) ; "make uninstall" le retire
$ make install

# Ou installe pour tout le système : v32opt dans /usr/local/bin et la page
# de manuel dans /usr/local/share/man/man1 (PREFIX=... change la racine ;
# DESTDIR=... est respecté pour préparer des paquets). "sudo make
# sysuninstall" retire les deux.
$ sudo make sysinstall

# ...avec la page de manuel en espagnol (es) ou en français (fr) au lieu
# de l'anglaise
$ sudo make sysinstall DEST_LANG=fr

# Lance la suite de tests unitaires (nécessite les DevTools de Vircon32
# -- assemble, packrom -- dans votre PATH ; le test primes nécessite en
# plus v32lua)
$ make tests

# Nettoie les produits de compilation ("make distclean" supprime aussi un
# répertoire build/ de CMake)
$ make clean
```

### Avec CMake (Linux / macOS / Windows)

CMake compile hors de l'arborescence des sources, dans un répertoire
`build/` (un `cmake .` dans l'arborescence est refusé, car il écraserait
le `Makefile` de base) :

```bash
$ mkdir build && cd build
$ cmake ..                    # compilation Release par défaut
$ cmake --build .
$ ctest                       # tests rapides (aucun outil Vircon32 nécessaire)
$ sudo cmake --install .      # voir les emplacements d'installation ci-dessous
```

| Plateforme | Exécutable `v32opt` | Page de manuel / documentation |
| --- | --- | --- |
| Linux, macOS (et autres Unix) | `/usr/local/bin/v32opt` | `/usr/local/share/man/man1/v32opt.1` |
| Windows (MSYS2 + MinGW) | `<Program Files>\Vircon32\v32tools\v32opt.exe` | `<Program Files>\Vircon32\v32tools\docs\v32opt\` (`README.md`, `v32opt.1`) |

Sous Windows, l'optimiseur est installé dans l'arborescence d'installation
par défaut de Vircon32, dans un répertoire `v32tools/` ; pensez donc à
ajouter l'entrée correspondante au `PATH`. Comme pour les DevTools,
compilez-le avec MSYS2 + MinGW (Visual C++ ne dispose pas de
`getopt_long`) ; depuis un shell MinGW :

```bash
$ mkdir build && cd build
$ cmake -G "MSYS Makefiles" ..
$ make
$ cmake --install .           # depuis un shell administrateur, pour Program Files
```

Autres réglages utiles :

```bash
$ cmake -DCMAKE_INSTALL_PREFIX=$HOME/.local ..   # installer ailleurs
$ cmake --install . --prefix /opt/v32opt         # ...ou le choisir à l'installation
$ cmake --build . --target uninstall             # annuler la dernière installation
$ cmake -DDEST_LANG=fr ..                        # documentation en français (fr) ou en espagnol (es)
$ cpack                                          # .tar.gz (+ .deb/.rpm sous Linux, .zip sous Windows)
```

Les variables de cache `V32OPT_INSTALL_BINDIR`, `V32OPT_INSTALL_MANDIR` et
`V32OPT_INSTALL_DOCDIR` modifient chaque destination séparément
(relativement au préfixe).

### Compilation directe

Vous pouvez aussi compiler directement le code modulaire avec GCC ou
Clang (les en-têtes se trouvent dans `inc/`, et les passes à lucarne dans
leur propre sous-répertoire) :

```bash
# Compile tous les fichiers sources avec l'optimisation activée
$ gcc -O2 -Wall -Wextra -Iinc src/*.c src/peephole/*.c -o v32opt

# Vérifie la compilation (le lancer sans arguments affiche l'aide
# d'utilisation)

$ ./v32opt
```

Une référence complète de la ligne de commande est disponible sous forme
de page de manuel unix dans `man/v32opt.1` (consultez-la avec
`man ./man/v32opt.1`).

### Langues de la documentation

Ce README et la page de manuel sont disponibles en anglais, en espagnol
et en français : `README.md` / `README.es.md` / `README.fr.md`, et
`man/v32opt.1` / `man/v32opt.es.1` / `man/v32opt.fr.1`
(`man ./man/v32opt.fr.1` pour en lire une sans l'installer). Les deux
compilations installent par défaut la page de manuel anglaise ;
`DEST_LANG=es` ou `DEST_LANG=fr` (`make sysinstall DEST_LANG=fr`,
`cmake -DDEST_LANG=fr ..`) installe cette traduction à la place, sous le
simple nom `v32opt.1`, de sorte que `man v32opt` l'affiche directement.
Là où CMake installe aussi le README (Windows), le README traduit est
installé sous le nom `README.md`.

### Binaires précompilés

Le workflow GitHub Actions `Generate Global Binaries`
(`.github/workflows/release-builds.yml`, à lancer manuellement depuis
l'onglet Actions) compile des binaires compressés en zip pour Linux
(x86_64, arm64, Raspberry Pi armv7/armv8), macOS (x86_64, arm64) et
Windows (x86_64, arm64), ainsi que -- en extras facultatifs qui ne
retardent jamais une publication -- OpenBSD (x86_64, arm64) et Haiku
(x86_64). Chaque zip contient le binaire, les README et les pages de
manuel. Avant la compilation, le workflow vérifie que les pages de manuel
portent la version courante (voir [Publication](#publication-estampillage-de-la-version)) ;
les compilations natives passent un test rapide (`--version` et
`testing/long-lines/run.sh`).

Une exécution de type `release` rassemble les zip, avec un
`SHA256SUMS.txt`, dans une publication (release) GitHub `v<VERSION>`, dont
le texte est `.github/release-notes.md` (`@VERSION@` est remplacé par la
version). La publication reste un **brouillon** tant que l'on n'appuie pas
sur « Publish release » dans la page Releases ; cochez `publish_release`
au lancement pour la publier aussitôt. Une version `-dev` est marquée
comme préversion (pre-release). Relancer l'exécution pour la même version
met à jour cette publication.

---

## Intégration dans le processus de construction d'une CART

La séquence habituelle de construction d'une CART Vircon32 comprend les
étapes suivantes (si vous écrivez en assembleur, vous pouvez commencer à
l'étape 2) :

### écrire/modifier le code source

Actuellement, Vircon32 fournit un compilateur C (considéré comme stable,
et langage principal de développement sur la plateforme) via sa suite
d'outils `DevTools`. Il existe aussi (en développement) des compilateurs
tiers pour Lua (`v32lua`) et C++ (`v32c++`, qui traduit le C++ en C pour
le compilateur C de Vircon32).

### compiler le code source

Chaque compilateur traduit le code et la syntaxe de son langage de haut
niveau en assembleur Vircon32. C'est cet assembleur qu'il vous faut avant
de pouvoir passer à l'optimisation avec `v32opt`.

```bash
# compile un programme C avec le compilateur C de Vircon32
$ compile -o game.asm game.c

# compile un programme lua avec le compilateur v32lua
$ v32lua -o game.asm game.lua

# compile un programme C++ : v32c++ produit du C, que le compilateur C compile ensuite
$ v32c++ -o game.c game.cpp
$ compile -o game.asm game.c
```

Le mode `-L` à utiliser (et les points d'attention) dépend de la
provenance de l'assembleur -- voir [Langages
Sources](#langages-sources--c-c-lua-et-assembleur).

Si vous écrivez un programme Vircon32 DIRECTEMENT en langage assembleur,
vous pouvez passer directement à l'étape suivante (optimiser).

Si vous voulez essayer d'optimiser une CART Vircon32 binaire déjà
empaquetée (peut-être n'avez-vous pas accès au code source pour la
reconstruire de zéro), vous pouvez utiliser les commandes `unpackrom` et
`disassemble` fournies par les `DevTools` de Vircon32 pour obtenir
l'assembleur désassemblé de n'importe quelle CART -- mais lisez d'abord
la mise en garde sur le désassemblage dans [Langages
Sources](#langages-sources--c-c-lua-et-assembleur) : la plupart des
optimisations cassent les programmes désassemblés.

Vérifiez que vous disposez bien du fichier `game.asm` obtenu (et, si vous
souhaitez noter le gain de place que l'optimisation permet, relevez la
taille totale du fichier d'assembleur).

### optimiser : faites passer votre fichier d'assembleur par v32opt

Une fois le fichier d'assembleur en main, faites-le passer par `v32opt`
en appliquant la combinaison d'optimisations souhaitée.

Il est recommandé d'essayer plusieurs exécutions, avec différentes
optimisations, pour trouver le « point idéal » recherché (le meilleur
gain de performances ou de place sans rien casser). Parfois vous aurez de
la chance et pourrez appliquer toutes les optimisations, et tout
fonctionnera. D'autres fois, il faudra revenir en arrière, isoler la ou
les optimisations problématiques et les exclure.

Pour commencer, essayez votre fichier d'assembleur avec `-O1`, en créant
un nouveau fichier d'assembleur `gameOpt.asm` (n'écrasez PAS
l'original) :

```bash
# fait passer game.asm par v32opt avec les optimisations de -O1 :
$ v32opt game.asm -o gameOpt.asm -O1
```

REMARQUE : le fichier d'entrée est le seul argument qui n'est pas une
option, et il peut apparaître avant, après ou parmi les options
(`v32opt -O1 -o gameOpt.asm game.asm` fonctionne de la même façon, sur
toutes les plateformes). Utilisez `-o` pour nommer la sortie ; sans
elle, `game.asm` est écrit dans `gameOpt.asm`. Un deuxième nom de fichier
est une erreur, au lieu d'être ignoré silencieusement.

Vous pouvez aussi lancer `v32opt` avec l'argument `-v` : il vous donnera
un compte rendu général des optimisations qu'il a pu effectuer :

```bash
# fait passer game.asm par v32opt avec les optimisations de -O1, avec statistiques :
$ v32opt game.asm -o gameOpt.asm -O1 -v
```

Vous devriez maintenant avoir `gameOpt.asm` (ou le nom que vous avez
choisi). C'est la forme « optimisée » de `game.asm`, résultat du travail
de `v32opt` sur celui-ci.

Comparez la taille des fichiers : le gain de place est un bénéfice
courant de l'optimisation, pour un effort relativement faible (dans la
plupart des cas, le fichier d'assembleur optimisé devrait être un peu
plus petit).

### poursuivre avec l'assemblage

À ce stade, vous pouvez poursuivre le processus de construction de votre
CART Vircon32 en faisant passer le code assembleur optimisé (dans
`gameOpt.asm`) par l'assembleur de Vircon32 pour obtenir le fichier objet
correspondant (`game.vbin` ou `gameOpt.vbin`) :

```bash
# assemble gameOpt.asm
$ assemble -o gameOpt.vbin gameOpt.asm
```

Si tout se passe bien, le fichier d'assembleur optimisé devrait
s'assembler aussi bien que le `game.asm` d'origine (on suppose : sans
problème).

### générer les ressources de la CART

Générez les textures et les sons à inclure dans votre CART, et créez
votre fichier de définition de CART `game.xml` (notez que `v32lua` génère
automatiquement un fichier XML par défaut) :

```bash
$ png2vircon -o background.vtex background.png
$ png2vircon -o sprites.vtex sprites.png
...
```

NOTEZ que votre fichier XML s'attend probablement à ce que le `.vbin`
porte le nom du fichier d'assembleur d'origine (`game.asm` ->
`game.vbin`) : renommez donc le fichier `gameOpt.vbin`, ou modifiez
`game.xml` pour qu'il utilise `gameOpt.vbin`. Ou, si les comparaisons de
performances vous intéressent, faites une copie du XML, nommez-la
`gameOpt.xml` et modifiez-la pour qu'elle utilise votre fichier optimisé.
Vous pourrez ainsi construire les deux (non optimisé, optimisé) et
comparer les résultats.

### empaqueter la CART

Enfin, une fois toutes les pièces en place (le fichier `.vbin`, les
éventuels fichiers `.vtex`/`.vsnd` et le fichier `.xml`), vous pouvez
construire la CART :

```bash
# construit la CART
$ packrom game.xml
```

À ce stade, en l'absence d'erreurs, vous devriez avoir un `game.v32`
jouable dans l'émulateur Vircon32.

---

## Langages Sources : C, C++, Lua et Assembleur

`v32opt` ne voit jamais que de l'assembleur, mais la façon dont cet
assembleur a été produit compte. Les passes reconnaissent les formes de
code et les conventions d'étiquettes qu'émettent les compilateurs
(étiquettes de fonction `__function_<nom>:`, leurs étiquettes internes
`..._return:`, le cadre standard `PUSH BP` / `MOV BP, SP`, les directives
`pointer` pour les adresses de fonctions, les en-têtes de boucle
`..._start:`), et le mode `-L` indique aux passes qui suivent les valeurs
comment interpréter les immédiats.

| Source | Chaîne d'outils | Mode `-L` | Remarques |
| --- | --- | --- | --- |
| C | `compile` (DevTools de Vircon32) | `c` (par défaut) | La cible principale ; toutes les passes sont conçues autour de cette sortie. |
| C++ | `v32c++` → C → `compile` | `c` (par défaut) | L'assembleur *est* une sortie du compilateur C, donc tout ce qui est dit pour le C s'applique tel quel (les vtables deviennent des tables `pointer`, que DCE suit déjà). |
| Lua | `v32lua` | `lua` | Permet aux passes de reconnaître les valeurs NaN-boxed de v32lua (voir ci-dessous). Traiter la sortie de v32lua en mode C peut replier, transférer ou éliminer par CSE l'arithmétique d'emballage/déballage (boxing/unboxing). |
| Assembleur écrit à la main | — | `c` (par défaut) | Fonctionne, avec les réserves ci-dessous. |
| CART désassemblée | `unpackrom` + `disassemble` | `c` | Sûr uniquement sans aucune passe qui change la taille -- voir ci-dessous. |

### Mode Lua (`-L lua`)

`-L lua` (forme longue `--langmode lua` ; le nom du mode est insensible à
la casse) rend les passes qui suivent les valeurs conscientes du système
de types emballés (boxed) de `v32lua` : les tournures d'étiquetage et de
désétiquetage `BOXED_*` (OR/AND/IADD avec un immédiat emballé) ne sont
jamais repliées, transférées ni éliminées par CSE comme de l'arithmétique
ordinaire ; les étiquettes de NaN-boxing (`BOXED_*`, `NAN_VALUE`, toute
valeur ayant le motif binaire de NaN) ne sont jamais résolues à travers
`%define` ; et la passe d'algèbre supprime la moitié « est-ce Nil ? »,
manifestement morte, du test de vérité fixe du compilateur après un
producteur de booléen emballé. `-L c` (la valeur par défaut) conserve le
comportement simple du mode C. Les étiquettes auxiliaires internes
`__global_*` de v32lua dans l'initialiseur de portée globale sont
comprises dans les deux modes.

### Assembleur écrit à la main

Utilisez le mode C par défaut. À savoir :

* **Les passes au niveau des fonctions se fient au nommage du
  compilateur.** DCE, l'intégration (inlining), l'élimination du pointeur
  de cadre et les passes promote-* ne traitent comme une fonction qu'une
  étiquette de la forme `__function_<nom>:`. Le code placé sous d'autres
  étiquettes n'est simplement jamais envisagé pour la suppression ou
  l'intégration -- c'est sûr, mais ces passes auront peu à faire à moins
  que vous ne suiviez la même convention.

* **Tout ce qui n'est atteint que par une adresse calculée est
  invisible.** Une routine `__function_` dont l'adresse n'est jamais
  formée qu'arithmétiquement, ou atteinte par `JMP R0`/`CALL R0` sans
  aucune référence `pointer`/d'opérande à son nom, peut être supprimée par
  DCE ; le code situé après un `JMP` inconditionnel dans lequel on n'entre
  que par un saut calculé (sans étiquette) est supprimé par
  `peephole-jumps`. Faire référence à l'étiquette par son nom n'importe où
  (`MOV R0, __function_cb`, une directive `pointer`) la maintient en vie.

* **Les étiquettes se terminant par `_start`** sont prises pour des
  en-têtes de boucle par la passe `promote-loops`. La sûreté de la boucle
  est jugée sur toute la `__function_` où elle se trouve (le code qui
  précède la première de ces étiquettes compte comme une seule
  fonction) : un seul `MOV R1, BP` ou `LEA R1, [BP-N]` dans celle-ci
  garde en mémoire toutes les boucles de cette fonction.

* Les directives de l'assembleur (`%define`, `%include`, directives de
  données, ...) ne sont jamais supprimées par aucune passe, et les
  valeurs entières des `%define` sont vues à travers le symbole, sauf si
  `-fno-resolve-defines` est indiqué.

### CARTs désassemblées

L'outil `disassemble` de Vircon32 nomme les cibles des sauts et des
appels (`_label1`, `_label2`, ...), mais toute référence à des *données*
dans la ROM du programme -- chaînes littérales, tables, valeurs
initiales -- reste une adresse codée en dur (`MOV R0, 0x2000002C`).
`v32opt` ne reloge pas ces adresses, donc **toute optimisation qui change
la taille du code déplace les données sous leurs références** et la CART
reconstruite lit la mauvaise mémoire. (Supprimer un seul
prologue/épilogue de cadre de 4 mots suffit à casser toutes les chaînes
du programme.) Considérez l'optimisation de code désassemblé comme une
expérience : comparez soigneusement la CART d'origine et la CART
optimisée, et attendez-vous à ce que seuls les programmes sans références
à des données résidant en ROM survivent. Comme les fonctions désassemblées
s'appellent `_labelN` et non `__function_<nom>`, DCE et l'intégration ont
de toute façon peu à y faire.

---

## Utilisation et Niveaux d'Optimisation

```bash
v32opt <entrée.asm> [-o sortie.asm] [options]
```

| Option | Description | Passes Incluses |
| --- | --- | --- |
| `-O0` | **Aucune Optimisation** | Désactive toutes les passes d'optimisation (par défaut). |
| `-O1` | **Lucarne Locale** | Active les 16 optimisations locales (14 à lucarne sur fenêtre, plus l'**écriture morte de registre** et le **nettoyage des copies**, guidés par la vivacité des registres). |
| `-O2` | **Analyse Globale** | Active toutes les passes de `-O1` + **CSE**, **Élimination du Code Mort (DCE)**, **Repliement Global des Constantes**, **Élimination du Pointeur de Cadre** et **Promotion de la Mémoire en Registres** (`promote-regs`, `promote-leaf`, `promote-loops`). |
| `-O3` | **Agressif** | Active toutes les passes de `-O2` + **Intégration des Fonctions (inlining)** + **Inversion de Boucles** + **Déroulage de Boucles** (complet et partiel) + **Repliement de l'Indice**. |
| `-Os` | **Gain de Place** | `-O3` sans inversion de boucles (qui duplique le test de chaque boucle) ; emplacement réservé pour un niveau dédié à la taille. |
| `-o <fichier>` | **Fichier de Sortie** | Où écrire l'assembleur optimisé (par défaut : `<entrée>Opt.asm`). |
| `-v`, `--verbose` | **Mode Verbeux** | Affiche des statistiques détaillées des passes et le nombre d'optimisations à chaque itération. |
| `-t` | **Mode Test** | Affiche un résumé `passe:nombre` lisible par une machine (plus une ligne `total:N`). |
| `-d` | **Marquage de Débogage** | Marque chaque optimisation dans la sortie même, par des commentaires `; [DEBUG <passe>] ...`. |
| `-L <c\|lua>`, `--langmode` | **Mode de Langage** | `lua` active la prise en compte des types NaN-boxed pour la sortie de `v32lua` ; `c` (par défaut) pour le C, le C++ et l'assembleur (voir [Langages Sources](#langages-sources--c-c-lua-et-assembleur)). |
| `--dot <fichier>` | **Export du CFG** | Exporte le Graphe de Flot de Contrôle dans un fichier `.dot` Graphviz pour le visualiser. |
| `--trigger-max=<N>` | **Plafond de Bissection** | Budget global du total des transformations appliquées (voir ci-dessous). |
| `--profile <fichier>` | **Boucles Guidées par Profil** | Un profil par lignes fait avec `scripts/v32prof.py` : les boucles chaudes sont déroulées davantage, les froides laissées intactes. |
| `-f<réglage>=N` | **Limites des Boucles** | Les limites de taille des passes de boucles (voir « Réglage des passes de boucles »). |
| `-V`, `--version` / `-h`, `--help` | **Information** | Affiche la version, ou un résumé de l'utilisation, puis quitte. |

`v32opt` n'affiche rien en cas de succès, sauf si `-v` ou `-t` est
indiqué, et se termine avec le code de sortie `1` (et un message sur
stderr) en cas d'option incorrecte, d'entrée manquante ou illisible,
ou de sortie impossible à écrire. (Les lignes d'entrée peuvent avoir
n'importe quelle longueur.)

### Philosophie des niveaux d'optimisation et considérations de débogage

Dans `v32opt`, les niveaux d'optimisation se distinguent non seulement
par l'ampleur de la réduction du binaire, mais aussi par leur complexité
de calcul, leur impact structurel et leur facilité de débogage :

* **`-O1` (Lucarnes Locales) :** Travaille sur de petites fenêtres
glissantes d'instructions (quelques passes regardent plus loin, dans une
limite de distance de recherche). Ce sont des passes sans état, en temps
linéaire, qui nettoient les artefacts évidents du compilateur sans impact
structurel sur le programme ni effet sur la facilité de débogage — le
niveau le moins risqué. Cela dit, « le moins risqué » ne veut pas dire
« sans risque » (voir `ISSUES` pour les limitations connues restantes) ;
il vaut donc toujours la peine de tester en jouant la version optimisée
face à l'originale.

* **`-O2` (Analyse Globale et Nettoyage Structurel) :** Introduit les
Graphes de Flot de Contrôle (CFG), le suivi du flot de données à
l'échelle du programme et des modifications du cadre de pile. Ces passes
analysent des blocs et des fonctions entiers pour éliminer le code
inaccessible, replier les constantes au travers des sauts, supprimer
les surcoûts redondants et garder les variables locales de pile dans des
registres (les passes `promote-*` ont besoin de connaître la fonction
entière -- quelles adresses de cadre s'échappent, quels registres sont
prouvés libres --, d'où leur place ici et non parmi les passes à
fenêtre glissante).

* **`-O3` (Transformations Agressives) :** Effectue des modifications profondes
qui échangent de la taille de binaire contre de la vitesse d'exécution :
l'intégration des fonctions, qui efface complètement les frontières entre
fonctions, et les passes de boucles (inversion, déroulage complet et partiel,
repliement de l'indice), qui copient du code. Leurs limites se règlent par
programme et peuvent être guidées par un profil mesuré (voir [USAGE.md](USAGE.md)).

#### Pourquoi l'Élimination du Pointeur de Cadre se trouve dans `-O2`

Du point de vue de la conception pure des compilateurs, l'Élimination du
Pointeur de Cadre (`omit_frame_pointers`) ressemble à une optimisation de
`-O1` typique : elle ne demande qu'un parcours linéaire rapide, elle
réduit *toujours* le nombre d'instructions et la taille du binaire, et
elle apporte un gain de vitesse immédiat aux fonctions feuilles.

Pourtant, nous la plaçons délibérément dans `-O2`. Les prologues de
fonction standard (`PUSH BP` suivi de `MOV BP, SP`) créent une liste
chaînée stable de cadres de pile en mémoire. Lorsque les pointeurs de
cadre sont supprimés, les émulateurs et les débogueurs ne peuvent plus
remonter depuis `BP` pour produire des traces propres de la pile d'appels
lors des plantages à l'exécution. En plaçant `omit_frame_pointers` dans
`-O2`, `-O1` reste un niveau performant tout en étant 100 % « sûr pour le
débogage » !

### Contrôle individuel des optimisations

Vous pouvez activer ou désactiver des passes précises avec `-f<nom>` et
`-fno-<nom>` :

```bash
# Exemple : lancer O2 mais désactiver les chaînes de sauts et ajouter la promotion en registres dans les boucles
$ v32opt game.asm -O2 -fno-peephole-jmp-chain -fno-promote-loops
```

---

## Phase 1 : Optimisations Locales à Lucarne (`-O1`)

La phase 1 travaille sur une fenêtre glissante d'instructions, en
nettoyant la sortie redondante du compilateur et en simplifiant des
séquences locales d'instructions. Les optimisations de cette catégorie
sont considérées comme les moins susceptibles de casser gravement le code
obtenu, tout en offrant un certain gain de place ou de performances.

Si vous débutez avec l'optimisation, essayez `-O1` et poursuivez votre
construction en comparant à la fois la taille du fichier ASM obtenu et le
comportement à l'exécution. Si vous voulez davantage d'optimisation (au
risque d'un assembleur cassé ou d'un comportement erroné à l'exécution),
vous pouvez passer aux niveaux d'optimisation supérieurs.

### Élimination de Paires d'Instructions Adjacentes (`peephole-pairs`)

Examine les paires consécutives pour supprimer les opérations
redondantes, comme les instructions de Conversion d'Entier en Booléen
(`CIB`) qui suivent immédiatement une comparaison destructive
(`IEQ`/`INE`, qui laissent déjà un `0` ou un `1` propre dans le registre
de destination), les doubles négations binaires (`BNOT`) et les
opérations de pile à effet net nul (`PUSH`/`POP`).

```vircon32
; AVANT                  ; APRÈS
IEQ R1, R2               IEQ R1, R2
CIB R1                   ; (CIB supprimé : IEQ produit déjà 0 ou 1)

BNOT R3
BNOT R3                  ; (La double négation s'annule)

PUSH R4
POP R4                   ; (Paire PUSH/POP supprimée)
```

Ceux qui connaissent l'assembleur verront qu'il s'agit en général de
suites inutiles, qui consomment des cycles du processeur sans rien
apporter à votre jeu. Les supprimer signifie moins de cycles consommés
par image (et cela *pourrait* améliorer les performances).

### Simplification Algébrique (`peephole-algebra`)

Supprime les copies d'un registre vers lui-même (`MOV r, r`) et
l'arithmétique neutre (`IADD`/`ISUB` avec `0`). Elle convertit aussi les
multiplications par 2 en additions du registre avec lui-même
(`IADD r, r`), par clarté idiomatique.

```vircon32
; AVANT                              ; APRÈS
MOV R1, R1                           ; (Copie vers soi-même supprimée)

IADD R2, 0                           ; (Addition neutre supprimée)

IMUL R3, 2                           IADD R3, R3
```

Comme pour l'élimination des paires, repérez les opérations mathématiques
évidentes qui ne produisent aucun changement. Elles peuvent être retirées
sans risque, ce qui réduit le nombre de cycles par image.

### Transfert d'Écriture vers Lecture (`peephole-forwarding`)

Lorsqu'une valeur est écrite en mémoire depuis un registre puis
immédiatement relue à la même adresse mémoire dans un autre registre, la
lecture mémoire est remplacée par une copie directe de registre à
registre.

```vircon32
; AVANT                              ; APRÈS
MOV [R1+4], R2                       MOV [R1+4], R2
MOV R3, [R1+4]                       MOV R3, R2
```

Même si cela n'apporte pas forcément de gain de performances notable,
cela devrait vous faire économiser 1 mot, car le `MOV` obtenu entre deux
registres ne nécessite qu'un mot pour stocker l'instruction, alors que
toute référence indirecte demande un second mot pour la valeur immédiate
ou l'adresse.

### Élimination des Rechargements Redondants (`peephole-compiler-myopia`)

Supprime le rechargement d'une valeur qui vient d'être écrite depuis le
même registre au même emplacement mémoire, lorsque rien entre-temps n'a
modifié la mémoire ni les registres concernés.

```vircon32
; AVANT                              ; APRÈS
MOV [R1+4], R2                       MOV [R1+4], R2
MOV R2, [R1+4]                       ; (Rechargement supprimé : R2 la contient toujours)
```

Un artefact courant du compilateur : le générateur de code réécrit une
valeur temporaire dans son emplacement de pile, puis la relit aussitôt,
avant que quoi que ce soit ait pu la modifier.

### Élimination des Sauts Redondants (`peephole-jumps`)

Trois nettoyages locaux du flot de contrôle : supprime les
`JMP`/`JT`/`JF` qui pointent directement vers l'étiquette qui suit
immédiatement l'instruction ; inverse les paires branchement par-dessus
un saut (`JF R, L1; JMP L2; L1:` devient `JT R, L2; L1:`) ; et élimine le
code rendu inaccessible par un `JMP` inconditionnel (jusqu'à l'étiquette
suivante, qui est conservée).

```vircon32
; AVANT                              ; APRÈS
JMP loop_continue                    ; (Saut vers l'instruction suivante supprimé)
loop_continue:                       loop_continue:
```

### Élimination des Copies Redondantes et Symétriques (`peephole-movs`)

Supprime les copies consécutives en double (`MOV R1, X; MOV R1, X`) et
les copies « en miroir », où deux registres échangent leurs valeurs deux
fois sans modification (`MOV R1, R2; MOV R2, R1`).

```vircon32
; AVANT                              ; APRÈS
MOV R1, 100                          MOV R1, 100
MOV R1, 100                          ; (Copie en double supprimée)

MOV R2, R3                           MOV R2, R3
MOV R3, R2                           ; (Copie en miroir supprimée)
```

### Combinaison des Opérations sur Immédiats (`peephole-immediates`)

Combine des additions ou soustractions successives sur le même registre
avec des opérandes immédiats en une seule opération combinée. Si les
opérations s'annulent à zéro, les deux sont supprimées.

```vircon32
; AVANT                              ; APRÈS
IADD R1, 5                           IADD R1, 2
ISUB R1, 3
IADD R2, 10                          ; (Les deux supprimées : +10 -10 = 0)
ISUB R2, 10
```

### Réduction de Force (`peephole-reduce`)

Simplifie les opérations arithmétiques avec des constantes particulières.
Les multiplications par `0` deviennent `MOV 0`, les multiplications ou
divisions par `1` sont supprimées, et les multiplications par `2` sont
converties en additions. Pour les formes en virgule flottante, seules les
identités exactes (`FMUL r, 1.0`, `FDIV r, 1.0`) sont appliquées —
`FMUL r, 0.0` est délibérément laissé tel quel, car `Inf/NaN * 0.0` vaut
`NaN`, et non `0.0`.

```vircon32
; AVANT                              ; APRÈS
IMUL R1, 0                           MOV R1, 0
IMUL R2, 1                           ; (Multiplication neutre supprimée)
IDIV R3, 1                           ; (Division neutre supprimée)
IMUL R4, 2                           IADD R4, R4
```

Sur bien d'autres processeurs, les instructions de multiplication et de
division peuvent être plus « coûteuses » en nombre de cycles. Sur
Vircon32, ce n'est pas un problème. Simplifier les opérations peut
néanmoins aider à la lisibilité générale.

### Optimisations des Décalages (`peephole-shifts`)

Supprime les décalages nuls par `0` et convertit les décalages à gauche
de `1` (`SHL r, 1`) en additions du registre avec lui-même (`IADD r, r`).

```vircon32
; AVANT                              ; APRÈS
SHL R1, 0                            ; (Décalage de 0 supprimé)

SHL R2, 1                            IADD R2, R2
```

### Repliement des Comparaisons à Zéro (`peephole-zero-test`)

Les sauts conditionnels de Vircon32 comparent déjà un registre à zéro
(`JT` saute quand il est non nul, `JF` quand il est nul) : calculer la
comparaison au préalable est donc redondant. Cette passe intègre la
comparaison dans le saut, aussi bien lorsqu'une copie est testée que
lorsque la valeur est testée sur place.

```vircon32
; AVANT                              ; APRÈS
MOV R0, R4
IEQ R0, 0
JT  R0, __slow_path                  JF  R4, __slow_path

INE R5, 0
JF  R5, __is_zero                    JF  R5, __is_zero
```

Le code d'origine laisse le résultat de la comparaison (0 ou 1) dans le
registre de travail ; la réécriture n'est donc appliquée que lorsqu'il
est prouvé que ce registre n'est plus jamais lu d'**aucun** côté du
branchement. La preuve suit les sauts et examine les fonctions appelées ;
tout ce qu'elle ne peut pas suivre (un saut ou un appel calculé, les
registres `R11`-`R15`, une fonction appelée susceptible de lire le
registre) laisse le code intact. Comparaisons entières uniquement : `FEQ`
traite `-0.0` comme zéro, alors que `JF` teste les bits bruts.

v32lua produit cette forme à chaque consultation de table en ligne : sur
les programmes en mode Lua, cette passe supprime donc environ 3 % de
toutes les instructions.

### Écritures Mortes de Registre (`peephole-dead-regs`)

La sortie du compilateur (et des passes promote) regorge d'instructions
qui n'écrivent qu'un registre que personne ne relit. Cette passe les
supprime.

```vircon32
; AVANT                              ; APRÈS
MOV  R1, R6                          ; (R1 n'est plus lu)
ILT  R0, R6                          ILT  R0, R6
MOV  R0, R4                          ; (R0 est réécrit en tête de boucle)
JMP  __loop                          JMP  __loop
```

Seules les instructions dont le seul effet est l'écriture sont candidates
(`MOV`, `LEA`, arithmétique, décalages, logique, comparaisons,
conversions) : jamais `IN`/`POP`, divisions, `MOD`, fonctions
transcendantes, `MOVS`/`SETS`/`CMPS` ni `CALL`. Elle n'est supprimée que
si `liveness.c` (la requête sensible au flot de contrôle de
`peephole-zero-test`) prouve qu'aucun chemin ne lit le registre. `R0` est
vivant à chaque `RET`, ainsi que tout registre qu'un appelant direct lit
après l'appel (l'auxiliaire de division non signée de v32cc renvoie le
reste dans `R1`).

### Nettoyage des Copies (`peephole-copies`)

Annule les copies de registres laissées une fois une valeur déplacée dans
un registre pour être traitée. Deux réécritures sur du code linéaire
(sans étiquette, saut ni appel) :

```vircon32
; AVANT                              ; APRÈS
MOV  R1, R4                          IADD R4, 1
IADD R1, 1
MOV  R4, R1                          ; (copie d'entrée, opération, copie de sortie)

MOV  R1, R6                          ; (R1 désormais mort : supprimé)
ILT  R0, R1                          ILT  R0, R6
```

*Coalescence :* `MOV Rt, Rs / ... / MOV Rs, Rt` autour d'une fenêtre qui
ne mentionne pas `Rs`, avec `Rt` mort ensuite, renomme la fenêtre sur `Rs`.
*Propagation :* après `MOV Rt, Rs`, les lectures de `Rt` jusqu'à la
prochaine écriture de l'un ou l'autre lisent `Rs` ; la copie disparaît si
`Rt` est mort. Avec `peephole-dead-regs`, c'est ce qui rend la promotion
rentable.

### Élimination des Écritures Mortes (`peephole-dead-stores`)

Supprime les écritures en mémoire immédiatement écrasées par une écriture
suivante à la même adresse mémoire indirecte, sans lecture
intermédiaire.

```vircon32
; AVANT                              ; APRÈS
MOV [R1+0], R2                       ; (Écriture écrasée supprimée)
MOV [R1+0], R3                       MOV [R1+0], R3
```

### Élimination des Lectures Redondantes (`peephole-loads`)

Lorsque deux registres lisent successivement la même adresse mémoire
indirecte, la seconde lecture est remplacée par une copie directe depuis
le registre de destination de la première.

```vircon32
; AVANT                              ; APRÈS
MOV R1, [R2+8]                       MOV R1, [R2+8]
MOV R3, [R2+8]                       MOV R3, R1
```

Là encore, le `MOV R3, R1` finira par économiser un mot, puisqu'il n'a
besoin de référencer aucune donnée immédiate.

### Repliement des Immédiats (`peephole-immediate-prop`)

Malgré son nom, cette passe replie des constantes *à l'intérieur* d'un
registre au lieu de les propager vers d'autres instructions. Elle fait
trois choses avec des immédiats entiers sur un registre de destination :

* supprime l'arithmétique neutre (`IADD`/`ISUB` par `0`, `IMUL`/`IDIV`
  par `1`) ;

* replie un `MOV` de constante dans un `IADD`/`ISUB`/`IMUL` ultérieur du
  même registre (jamais au travers d'une lecture ou d'une écriture de ce
  registre, d'un branchement conditionnel, ni d'une frontière
  étiquette/saut/appel) ;

* fusionne des immédiats `IADD`/`ISUB` adjacents sur le même registre, en
  supprimant purement et simplement la paire quand ils s'annulent.

```vircon32
; AVANT                              ; APRÈS
MOV R1, 10                           MOV R1, 15
IADD R1, 5                           ; (Replié dans le MOV)

IADD R2, 5                           IADD R2, 2
ISUB R2, 3                           ; (Fusionné)
```

Un repliement dont le résultat ne tiendrait pas dans un littéral de
32 bits (l'arithmétique a débordé) est laissé tel quel. `MOV R1, 42` /
`IADD R2, R1` n'est *pas* réécrit en `IADD R2, 42` -- cela coûterait un
mot au lieu d'en économiser un.

### Élimination des Chaînes de Sauts (`peephole-jmp-chain`)

Raccourcit l'indirection des sauts. Tout `JMP`, `JT` ou `JF` -- où qu'il
se trouve dans le programme -- dont l'étiquette cible mène directement à
un autre `JMP <étiquette>` inconditionnel est redirigé vers le bout de la
chaîne, quel que soit le nombre de sauts. Une fois le dernier utilisateur
de l'étiquette d'un saut intermédiaire redirigé, ce saut intermédiaire
est supprimé.

```vircon32
; AVANT                              ; APRÈS
JT  R0, label_step1                  JT  R0, label_final
...                                  ...
JMP label_step1                      JMP label_final
...                                  ...
label_step1:                         label_step1:
JMP label_step2                      ; (Saut intermédiaire supprimé)
label_step2:                         label_step2:
JMP label_final                      ; (Saut intermédiaire supprimé)
```

« Mène directement à » tolère d'autres étiquettes, des commentaires et
des lignes vides entre l'étiquette et son `JMP` ; toute directive met fin
à la recherche. Une chaîne qui reboucle sur elle-même (`L: JMP L`) est
laissée telle quelle, et une chaîne qui se termine par un saut calculé
(`JMP R0`) n'est suivie que jusqu'à sa dernière étiquette.

REMARQUE : un saut intermédiaire n'est supprimé que lorsque plus rien ne
peut l'atteindre : aucune étiquette située juste au-dessus n'est
référencée nulle part dans le programme (comme cible de saut ou d'appel,
par un `MOV R0, étiquette` qui en prend l'adresse, par une directive
`pointer`, ...), et aucun code ne peut y tomber (l'instruction précédente
doit elle-même être un transfert inconditionnel ; un saut tout au début
du programme est le point d'entrée du démarrage et est toujours
conservé). Sinon, il est conservé et continue d'acheminer correctement
ses utilisateurs restants.

---

## Phase 2 : Flot de Données Global et Code Mort (`-O2`)

La phase 2 construit un **Graphe de Flot de Contrôle (CFG)** sur les
blocs de base pour réaliser une analyse de flot de données à l'échelle du
programme.

### Élimination des Sous-expressions Communes (CSE)

**Ce qu'elle fait :** Détecte et élimine les calculs redondants à
l'intérieur des blocs de base. Lorsque la même opération est appliquée
aux mêmes opérandes sources (après des initialisations `MOV`
identiques), la seconde occurrence est remplacée par un `MOV` depuis le
registre du premier résultat. Sur Vircon32, où toutes les instructions
prennent 1 cycle, cela **réduit la taille du code** en éliminant des mots
en double.

---

#### **Motif**
```
MOV Rx, A    ; Calcule Rx = A
OP Rx, B     ; Calcule Rx = Rx OP B
...
MOV Ry, A    ; Réinitialise Ry = A (comme Rx)
OP Ry, B     ; -- CSE remplace ceci par : MOV Ry, Rx
```

---

#### **Exemples**

| Avant | Après | Gain |
|--------|-------|---------|
| `MOV R1, R5`<br>`IADD R1, R2`<br>`MOV R3, R5`<br>`IADD R3, R2` | `MOV R1, R5`<br>`IADD R1, R2`<br>`MOV R3, R5`<br>`MOV R3, R1` | 0 mot à elle seule (opération d'1 mot → `MOV` d'1 mot) ; voir ci-dessous |
| `MOV R1, R5`<br>`IMUL R1, 42`<br>`MOV R3, R5`<br>`IMUL R3, 42` | `MOV R1, R5`<br>`IMUL R1, 42`<br>`MOV R3, R5`<br>`MOV R3, R1` | **1 mot** (`IMUL` + immédiat = 2 mots, `MOV` = 1) |
| `MOV R1, R5`<br>`FADD R1, R2`<br>`MOV R3, R5`<br>`FADD R3, R2` | `MOV R1, R5`<br>`FADD R1, R2`<br>`MOV R3, R5`<br>`MOV R3, R1` | 0 mot à elle seule ; voir ci-dessous |

Le `MOV R3, R5` de réinitialisation devient mort (il est immédiatement
écrasé), et à `-O2` `peephole-dead-stores` le supprime -- de sorte que,
lors d'une exécution complète de `-O2`, chaque ligne ci-dessus économise
un mot de plus.

---

#### **Opérations Prises en Charge**

Toutes les opérations arithmétiques, logiques et en virgule flottante :

`IADD`, `ISUB`, `IMUL`, `IDIV`, `AND`, `OR`, `XOR`, `FADD`, `FSUB`,
`FMUL`, etc. Fonctionne avec des registres et des immédiats (y compris
négatifs), et respecte :

- ✅ Les frontières du flot de contrôle (`JMP`, `JT`, `JF`, `CALL`, `RET`, étiquettes)
- ✅ Les modifications de registres entre les expressions
- ✅ Les opérations différentes (p. ex. `IADD` ≠ `ISUB`)

### Élimination des Fonctions Mortes (`dce`)

Effectue une analyse d'accessibilité à partir des racines connues du
programme -- le code situé avant la première fonction (le chemin de
démarrage), `__function_main` / `main` / `_start` / `start` / `__start`,
les routines d'initialisation globale (`__init_globals`,
`__function_init`, l'initialiseur de portée globale de v32lua), les
étiquettes contenant `ISR` ou `interrupt`, les étiquettes de données, et
toute fonction nommée dans une directive `pointer`. Toute fonction dont
le nom apparaît comme opérande d'un code accessible (un `CALL`, ou un
`MOV R0, __function_cb` qui en prend l'adresse) devient accessible à son
tour ; les fonctions (étiquettes de la forme `__function_<nom>:`) jamais
atteintes sont supprimées.

```vircon32
; AVANT                              ; APRÈS
__function_main:                     __function_main:
    CALL __function_init                 CALL __function_init
    RET                                  RET

__function_unused:                   ; (Fonction inaccessible supprimée)
    MOV R1, 0
    RET
```

Utile surtout pour le compilateur lua, car les versions récentes du
compilateur C de Vircon32 effectuent déjà une forme d'élimination des
fonctions mortes pendant la compilation (le C++ passe par ce même
compilateur C).

### Propagation et Repliement Global des Constantes (`constant-folding`)

Utilise un algorithme à liste de travail fondé sur un treillis, sur le
CFG, pour suivre les constantes connues des registres d'un bloc à
l'autre. Il replie l'arithmétique constante et remplace les références à
des registres par des valeurs immédiates au travers des sauts et des
branchements, en invalidant l'état de façon sûre lors des appels de
fonction (`CALL`) ou des écritures indirectes.

```vircon32
; AVANT                              ; APRÈS
block_1:                             block_1:
    MOV R1, 10                           MOV R1, 10
    JMP block_2                          JMP block_2
block_2:                             block_2:
    MOV R2, R1                           MOV R2, 10
```

Notez que cela échange des mots contre des immédiats (`MOV R2, 10` occupe
deux mots là où `MOV R2, R1` en occupait un) : à elle seule, cette passe
peut donc *agrandir* le binaire assemblé ; son intérêt réside dans les
optimisations à lucarne fondées sur les immédiats qu'elle rend possibles
ensuite.

### Élimination du Pointeur de Cadre (`omit-frame-pointers`)

Examine des corps de fonction entiers pour vérifier si le registre de
Pointeur de Base (`BP`) est un jour référencé ou déréférencé par les
instructions de la fonction. Dans les fonctions où `BP` n'est jamais
touché (comme de petites fonctions mathématiques feuilles, des accesseurs
ou des fonctions n'utilisant que les registres généraux `R0–R15`), elle
supprime entièrement le prologue standard de la fonction (`PUSH BP`,
`MOV BP, SP`) et son épilogue (`MOV SP, BP`, `POP BP`). Toute référence
relative à BP compte comme « toucher » BP — les emplacements de
paramètres (`[BP+N]`) COMME ceux des variables locales (`[BP-N]`), puisque
les deux accèdent à la mémoire au travers du pointeur de cadre.

Cela supprime 4 instructions redondantes de pile et de mémoire à chaque
appel, réduisant sensiblement le surcoût en cycles et la taille totale du
binaire sans modifier la logique des registres.

```vircon32
; AVANT                  ; APRÈS
__function_add:          __function_add:
    PUSH BP                  ; (Mise en place du cadre dans le prologue supprimée)
    MOV BP, SP               ;
    IADD R1, R2              IADD R1, R2
    MOV R0, R1               MOV R0, R1
    MOV SP, BP               ; (Libération du cadre dans l'épilogue supprimée)
    POP BP                   ;
    RET                      RET
```

---

## Phase 3 : Intégration et Optimisation des Boucles (`-O3`)

### Intégration des Fonctions (`inline`)

Repère les fonctions triviales (courte durée d'exécution et flot de
contrôle simple) et remplace leurs appels `CALL` directement par le corps
de la fonction. Cela supprime le surcoût des sauts d'appel et de retour
et fait apparaître de nouvelles occasions d'optimisation à lucarne au
point d'appel.

> **Options de diagnostic :** Limitez l'intégration avec
> `-finline-max=N` ou excluez des fonctions précises avec
> `-finline-exclude=<nom>`.

```vircon32
; AVANT                              ; APRÈS
CALL __function_add_one              IADD R1, 1
...                                  ...
__function_add_one:                  __function_add_one:
    IADD R1, 1                           IADD R1, 1
    RET                                  RET
```

Cette optimisation s'est révélée la plus efficace en matière de gains
appréciables (de performances comme de place, du fait qu'elle peut
supprimer quantité d'instructions inutiles liées à la mise en place et à
la libération d'une fonction). Mais elle est aussi considérée comme assez
*agressive* par l'ampleur des modifications qu'elle apporte à votre code.


### Inversion de Boucles (`loop-invert`)

Les deux compilateurs produisent des boucles testées en tête : chaque
itération exécute donc deux sauts, le conditionnel du test et le saut de
retour. L'inversion transforme le saut de retour en une copie du test suivie
d'un saut conditionnel direct vers le corps, ce qui économise un saut (un
cycle) par itération :

```vircon32
; AVANT                           ; APRÈS
__for_1_start:                    __for_1_start:
    MOV R0, R1                        MOV R0, R1
    ILT R0, R2                        ILT R0, R2
    JF  R0, __for_1_end               JF  R0, __for_1_end
    ...corps...                   __v32opt_loop_1:
    JMP __for_1_start                 ...corps...
__for_1_end:                          MOV R0, R1
                                      ILT R0, R2
                                      JT  R0, __v32opt_loop_1
                                  __for_1_end:
```

La copie du test en tête est conservée : l'entrée dans la boucle et tout autre
saut vers le début (un `continue` dans un `while`) voient donc toujours le
test d'abord. Une boucle exécutée zéro ou une fois ne coûte rien de plus ;
chaque itération supplémentaire économise un cycle, au prix de la taille du
test une fois (8 instructions au plus).

C'est une simple duplication de queue du code à la cible du saut : aucun
registre n'est lu ou écrit qui ne l'était pas avant. Un saut de retour n'est
réécrit que si le test est du code linéaire (ni étiquette, ni `CALL`, ni autre
branchement), si le saut est arrière, si le code qui suit le saut commence par
l'étiquette de sortie du test, si les étiquettes ne sont définies qu'une fois
(les étiquettes `.local` par défaut de v32cc se répètent par fonction :
utilisez `--standard`) et si la cible n'est pas une entrée de fonction. Elle
s'exécute une fois, après les passes promote (qui reconnaissent une boucle à
son saut de retour) et avant le nettoyage final. Elle fait partie de `-O3`,
pas de `-O2` ni de `-Os`, car elle échange de la taille de code contre de la
vitesse ; `-floop-invert` l'active seule.


### Déroulage Complet des Boucles (`loop-unroll`)

Une boucle comptée dont le nombre d'itérations est une constante de
compilation est remplacée par autant de copies de son corps, sans test ni
saut de retour (voir l'exemple de la version anglaise) :

```vircon32
; AVANT                           ; APRÈS (4 itérations)
    MOV R4, 0
__for_1_start:                        ...corps, R4 = 0...
    MOV R0, R4                        IADD R4, 1
    ILT R0, 4                         ...corps, R4 = 1...
    JF  R0, __for_1_end               IADD R4, 1
    ...corps...                       ...                (2 copies de plus)
    IADD R4, 1                    __for_1_end:
    JMP __for_1_start
__for_1_end:
```

À chaque itération, on supprime le test (3 instructions) et le saut (1).
Chaque copie garde son `IADD Ri, pas` : le compteur vaut partout la même
valeur qu'avant, et dans chaque copie les lectures du compteur là où un
immédiat est accepté sont remplacées par sa valeur. Conditions : le nombre
d'itérations est calculé à partir de la valeur initiale et du pas constants ;
le corps n'écrit pas le compteur et ne contient pas de `CALL` ; l'étiquette de
début n'est référencée que par le saut de retour et les étiquettes du corps
que depuis l'intérieur (les copies reçoivent des étiquettes neuves) ; le
registre du test est recréé (`MOV Rc, 1` en tête de chaque copie, `MOV Rc, 0`
à la sortie normale) quand la vivacité montre que quelqu'un le lit. 16
itérations et 96 instructions au plus ; les boucles imbriquées se déroulent
de l'intérieur vers l'extérieur. Elle s'exécute une fois, après le repliement
des constantes et avant `loop-invert` ; elle fait partie de `-O3` (pas de
`-Os`, car le code grossit) et `-floop-unroll` l'active seule.


### Repliement de l'Indice de Boucle (`loop-index`)

Le code à base de tableaux adresse les éléments par `base + i`. Quand le
compteur d'une boucle comptée ne sert qu'à former des adresses d'une même
base, cette base est repliée dans le compteur lui-même, sans registre
supplémentaire (voir l'exemple de la version anglaise) : un `IADD Ri, BASE` est
ajouté une fois avant la boucle, la borne immédiate est décalée de BASE à la
compilation et chaque paire `MOV Rt, BASE / IADD Rt, Ri` devient
`MOV Rt, Ri`. Une instruction de moins par usage et par itération.
Conditions : toute autre mention du compteur est la seconde moitié d'une paire
adjacente de cette même base (un nombre ou un `%define` qui s'y résout) ; la
borne est immédiate ; le corps n'a pas de `CALL` ; le compteur est mort à
l'étiquette de fin, à chaque saut vers l'extérieur et à chaque `RET` ; et
l'étiquette de début n'est atteinte que par continuation. Elle s'exécute une
fois, après `loop-unroll` et avant `loop-invert` ; elle fait partie de `-O3`,
et `-floop-index` l'active seule.

### Déroulage Partiel des Boucles (`loop-unroll-partial`)

Une boucle comptée dont le nombre d'itérations n'est **pas** connu à la
compilation paie son test et son saut à chaque itération. Cette passe exécute K
itérations (4, ou 2 pour un corps plus grand) par tour d'une nouvelle boucle
principale, derrière une seule garde qui vérifie que les K passeront le test, et
garde la boucle d'origine pour finir les 0..K-1 itérations restantes (voir
l'exemple de la version anglaise). Sur 4 itérations, 3 tests et 3 sauts
disparaissent. La garde n'utilise que le registre du test d'origine : une
borne immédiate est décalée à la compilation ; une borne en registre ou en
emplacement de cadre utilise `MOV Rc, borne / ISUB Rc, (K-1)*pas /
I<miroir> Rc, Ri`. Conditions : la comparaison est cohérente avec le sens du
pas (`ILT`/`ILE` avec un pas positif, `IGT`/`IGE` avec un pas négatif ; `INE`
est laissé) ; la borne ne change pas dans le corps (immédiate ; registre que le
corps n'écrit pas ; emplacement `[BP+N]` qu'aucune instruction du corps
n'écrit, dans une fonction dont l'adresse de cadre ne s'échappe jamais) ; le
corps n'a pas de `CALL`, n'écrit pas le compteur hors de son pas et compte au
plus 24 instructions ; les registres temporaires du test sont morts à l'entrée
du corps, à l'étiquette de fin et à chaque saut vers l'extérieur ; les
étiquettes du corps ne sont référencées que de l'intérieur et reçoivent de
nouveaux noms dans chaque copie. Elle s'exécute après `loop-unroll` et
`loop-index`, avant `loop-invert` ; elle fait partie de `-O3`, et
`-floop-unroll-partial` l'active seule.


### Réglage des passes de boucles

Les limites des passes de boucles sont des options de ligne de commande
(`-f<nom>=N`) : chaque programme peut ainsi être amené à son propre optimum
(`testing/bench/bench.py --tune` automatise la recherche) :
`loop-unroll-max-trips` (16, itérations que `loop-unroll` supprime
entièrement), `loop-unroll-max-size` (96, instructions de la boucle déroulée),
`loop-unroll-partial-factor` (0, copies par tour, 2..8 ; 0 = 4, ou 2 pour les
gros corps), `loop-unroll-partial-max-body` (24), `loop-unroll-partial-big-body`
(12), `loop-unroll-partial-min-body` (1) et `loop-invert-max-test` (8, test le
plus long que `loop-invert` duplique). Les valeurs sont bornées.


### Passes de boucles guidées par profil (`--profile`)

Le déroulage échange de la taille de code contre de la vitesse, et le gain dépend
de la « chaleur » réelle de la boucle. Avec un profil mesuré, `v32opt` cesse de
deviner :

```bash
scripts/v32prof.py jeu.asm -o jeu.prof --frames 600       # l'exécute dans v32sim, compte chaque ligne
v32opt jeu.asm -O3 --profile jeu.prof -o jeuOpt.asm         # le MÊME fichier
```

`v32prof.py` assemble le programme avec les infos de débogage, l'exécute sans
écran dans `v32sim` (`--profile-out`, `--fixed-clock`, `--seed=1`, `--fixed-cycles`)
et reporte les compteurs par adresse sur les lignes du fichier. Pour chaque
boucle, la part de toutes les instructions exécutées entre son étiquette de début
et son saut de retour décide : **chaude** (au moins `-fprofile-hot=PCT`, 2 % par
défaut) : `loop-unroll` peut supprimer deux fois plus d'itérations et grossir deux
fois plus, et `loop-unroll-partial` prend le facteur 8 (s'il est automatique) et
des corps deux fois plus grands ; **froide** (sous `-fprofile-cold=PCT`, 0,1 %) :
`loop-invert`, `loop-unroll` et `loop-unroll-partial` la laissent telle quelle ;
sinon, les limites habituelles s'appliquent. Une boucle créée par une passe, ou
que le profil ne couvre pas, n'est ni chaude ni froide. Un profil qui ne
correspond pas au fichier est signalé puis ignoré. Nécessite un `v32sim` avec
`--profile-out`.

---

## Promotion de la Mémoire en Registres (`-O2`)

Le compilateur C de Vircon32 garde chaque variable locale en mémoire
(`[BP-1]`, `[BP-2]`, ...) et passe par un registre à chaque usage. Trois
passes déplacent les locales les plus utilisées dans des registres libres
(`R1`-`R10`) sur la portion de code où c'est sûr. Seule, elle coûte les
mêmes cycles ; ce qui rapporte, c'est que les copies qu'elle laisse sont
ensuite annulées par les passes `-O1` `peephole-copies` et
`peephole-dead-regs`. Les trois font partie de `-O2` (donc de `-O3` et
`-Os`) ; `-fno-promote-regs`, `-fno-promote-leaf` et `-fno-promote-loops`
en écartent une.

### Promotion des Emplacements de Pile (`promote-leaf` / `promote-regs`)

* `promote-leaf` travaille sur des **fonctions feuilles** entières (sans
  `CALL`) : la lecture est placée juste après le prologue.
* `promote-regs` applique la même logique aux fonctions ordinaires, séparément
  dans chaque tronçon sans `CALL`, et seulement s'il a une entrée et une
  sortie. Une locale écrite dans un tronçon est réécrite juste avant le
  `CALL` qui le termine, si quelque chose la lit ensuite.

Rien n'est réécrit avant `RET` : le cadre est mort et aucun pointeur ne
peut le viser (voir plus bas).

### Promotion en Registres Invariante dans les Boucles (`promote-loops`)

Agit sur les boucles sans `CALL` et promeut les locales utilisées dans le
corps : la lecture va avant l'étiquette d'en-tête (un vrai préheader) et
l'écriture après l'étiquette de sortie. La lecture est omise si tout
chemin écrit le registre avant de le lire, l'écriture si rien hors de la
boucle ne lit l'emplacement (sauf boucle imbriquée, qui est ré-entrée). Les
références sont pondérées par l'imbrication (x8 par niveau) : avec peu de
registres, les boucles les plus internes gagnent. Une boucle non
promouvable est écartée seule ; ses boucles imbriquées restent examinées.

### Convention d'appel : à qui sont les registres ?

Le compilateur standard de Vircon32 est **callee-save** : une fonction
sauvegarde (`PUSH`) chaque registre qu'elle utilise et le restaure avant
`MOV SP, BP`, car ses appelants gardent des temporaires vivants dans `R1`,
`R2`, ... à travers un `CALL`. Une locale promue dans un registre que la
fonction n'a pas sauvegardé détruirait la valeur de l'appelant. Donc, hors
`-L lua` :

* un registre que la fonction sauvegarde déjà s'utilise gratuitement ;
* sinon, un `PUSH` est ajouté après les existants et un `POP` avant les
  existants sur **chaque** sortie (seulement si toutes les sorties sont des
  épilogues reconnaissables et que les références justifient les 2
  instructions de plus ; `R4`-`R10`) ;
* sinon, la locale reste en mémoire.

`-L lua` est **caller-save** (v32lua sauvegarde autour des appels) : rien
à sauvegarder. La sortie de v32cc est traitée comme celle du compilateur
standard : au pire une paire inutile, jamais un résultat faux.

### Ce qui garde un emplacement en mémoire

Ces contrôles portent sur **toute la fonction**, pas seulement la région :

* L'adresse du cadre s'échappe : `MOV R1, BP`, `LEA R1, [BP-N]`,
  `IADD R1, BP`, ... ; de même pour toute copie de `SP`.
* Pas de prologue `MOV BP, SP`.
* Un tronçon ou une boucle où quelque chose saute de l'extérieur, ou de
  l'intérieur vers un point qui éviterait l'écriture.
* Les paramètres (`[BP+N]`) ne sont jamais promus.

### Choix du registre

Un registre n'est utilisé que s'il est **prouvé libre** : absent de la
région, et la requête de vivacité (`liveness.c`) montre qu'il n'est lu
avant réécriture sur aucun chemin depuis la lecture. `R11`-`R13` (opérandes
implicites de `MOVS`/`SETS`/`CMPS`) ne sont jamais utilisés.

### À quoi s'attendre

Sur environ 190 programmes générés par compilateur (et ~175 de plus via le
compilateur standard), cycles / taille par rapport à `-O0` :

| | v32cc | `compile` standard |
| --- | --- | --- |
| `-O2` avant promote et nettoyage | -1,6 % / -3,1 % | -1,6 % / -5,6 % |
| `-O1` (désormais avec le nettoyage) | -3,4 % / -3,8 % | -3,0 % / -6,1 % |
| `-O2` | -6,9 % / -6,0 % | -4,9 % / -14,5 % |
| `-O3` | -7,9 % / -6,0 % | -7,0 % / -14,4 % |

`-d` marque chaque lecture et écriture qu'une passe ajoute
(`; [DEBUG promote-regs] ...`), et `--trigger-max` compte un emplacement
par variable promue : un défaut se bissecte comme avec toute autre passe.

---

## Visualisation du CFG

### Exporter les Graphes de Flot de Contrôle

Vous pouvez produire des diagrammes du graphe de flot de contrôle de
votre assembleur avec le paramètre `--dot` :

```bash
v32opt game.asm -O2 --dot cfg.dot
dot -Tpng cfg.dot -o cfg.png
```

Cela exporte les blocs, les étiquettes, les listes d'instructions et les
arcs de branchement orientés (en gérant les passages à l'instruction
suivante, les sauts inconditionnels, les branchements conditionnels
(`JT`/`JF`) et les appels) au format Graphviz standard.

---

## Garde-fous de Sûreté et de Correction

Pour éviter que les optimisations n'altèrent la sémantique du programme
ou ne cassent des comportements dans des cas limites, l'optimiseur impose
des vérifications structurelles de sûreté strictes :

* **Préservation des littéraux en virgule flottante :** Les immédiats en
virgule flottante (p. ex. `0.500000`) sont explicitement distingués des
immédiats entiers. Les passes de suivi des constantes traitent les
littéraux flottants comme inconnus (`VAL_BOTTOM`) au lieu de les
interpréter comme des `0` entiers. Cela évite que le repliement des
constantes ne corrompe silencieusement des arguments en virgule
flottante (comme les volumes des canaux audio ou des calculs physiques).

* **Plage des littéraux :** Tout repliement de constantes est calculé en
précision étendue et abandonné si le résultat ne tient pas dans un
littéral de 32 bits accepté par l'assembleur de Vircon32 : une chaîne
`IMUL`/`IADD` qui déborde est donc laissée telle qu'écrite au lieu de
produire un immédiat hors limites.

* **Les directives ne sont jamais supprimées :** Aucune passe ne supprime
une directive de l'assembleur (`%define`, `%include`, données
`integer`/`float`/`string`/`pointer`/`datafile`), même lorsqu'elle se
trouve dans une plage de code mort en cours de suppression. Une ligne peut
avoir n'importe quelle longueur : le texte de chaque ligne est conservé en
entier (un long littéral `string`, ou une table `pointer` de centaines de
fonctions, tient sur une seule ligne), et DCE lit une ligne `pointer` en
entier, si bien qu'une fonction nommée uniquement à son extrémité est
quand même conservée.

* **Protection des lectures autoréférentielles :** L'élimination des
copies redondantes examine en profondeur les lectures mémoire indirectes.
Des instructions textuellement identiques comme `MOV R1, [R1]` suivie
d'un second `MOV R1, [R1]` sont reconnues comme un enchaînement de
déréférencements de pointeurs et non comme des doublons, ce qui garantit
que les accès de pointeur à pointeur sont évalués correctement.

* **Garde-fous contre la prise d'adresse :** Les passes de promotion en
registres recherchent dans la fonction entière (et pas seulement dans la
boucle ou le tronçon promu) des calculs d'adresses de pile (p. ex.
`IADD R1, BP`, `LEA R1, [BP-N]`, `MOV R1, SP`). Si l'adresse mémoire
d'une variable locale est calculée dynamiquement ou prise n'importe où
dans celle-ci, la promotion est automatiquement annulée pour cette
fonction, afin de garantir la sûreté mémoire et d'éviter les bogues
d'alias. Le choix du registre s'appuie sur une preuve de vivacité (voir
[Promotion de la Mémoire en Registres](#promotion-de-la-mémoire-en-registres--o2)).

* **Réécriture de la pile lors de l'intégration :** Lorsque des fonctions
feuilles sont intégrées, les lectures de paramètres accédant à `[BP+N]`
(avec `N >= 2`) sont réécrites dynamiquement en `[SP+(N-2)]` au point
d'appel. Cela permet d'insérer le corps des fonctions appelées sans
corrompre les cadres de pile de l'appelant ni nécessiter de pointeur de
cadre dédié.

* **Ces garde-fous ne sont pas exhaustifs.** Les passes à fenêtre
modélisent désormais les instructions de lecture-modification-écriture en
mémoire (`IADD [R1+0], 5`), les écritures par alias au travers de
registres de base différents et les opérations de mémoire dynamique
`MOVS`/`SETS`/`CMPS`, et `CALL`/`RET` sont traités comme modifiant SP
(l'empilement et le dépilement matériels de l'adresse de retour). Les
limitations connues restantes sont consignées dans le fichier `ISSUES`
(notamment : l'élimination du code inaccessible ne cherche pas à
raisonner sur les cibles de sauts calculées — `JMP R0` / `JMP 0x1000` —
et l'heuristique du suffixe `_return:` de DCE peut se tromper sur une
vraie fonction dont le nom se termine par hasard par `_return`). Vérifiez
toujours la sortie optimisée en la construisant et en l'exécutant à côté
de l'originale.

---

## Options de Diagnostic et de Débogage

Lorsque vous travaillez sur des bases de code complexes ou isolez des
problèmes d'exécution causés par des transformations agressives, utilisez
les options de diagnostic suivantes pour contrôler le comportement de
l'optimiseur :

* **`-finline-max=N`**

Plafonne la taille d'une fonction encore considérée comme intégrable, en
instructions du corps : toute fonction dont le corps linéaire dépasse `N`
instructions n'est jamais intégrée. La valeur est bornée à `0..32` (la
capacité de la table des candidates).

*(Par défaut : `8`).*

* **`-finline-call-limit=N`**

Plafonne à `N` le nombre total d'appels `CALL` intégrés sur l'ensemble de
l'exécution (évalués dans l'ordre du fichier). En ajustant ce nombre,
vous pouvez procéder par bissection sur les appels intégrés pour isoler
des bogues qui n'apparaissent qu'à l'exécution.

*(Par défaut : `-1`, c'est-à-dire aucune limite).*

* **`-finline-exclude=NOM`**

Exclut des fonctions précises de l'intégration. Accepte une liste de noms
d'étiquettes séparés par des virgules.

*(Exemple : `-finline-exclude=__function_play_audio,__function_update_physics`).*

* **`-fmax-passes=N`**

Limite le moteur itératif d'optimisation locale à un maximum de `N`
passes (par défaut : `1000`).

* **`--trigger-max=N`**

Un budget global de transformations partagé par TOUTES les passes
activées et par chaque itération de point fixe : une fois `N`
transformations appliquées au total (nœuds supprimés, réécritures sur
place, lectures/écritures insérées), tout candidat ultérieur est laissé
intact, comme s'il n'avait jamais correspondu. Omettez-la (ou passez un
`N` négatif) pour ne pas avoir de limite. C'est l'outil principal pour
localiser par bissection une compilation erronée : relancez avec des
valeurs croissantes de `N` jusqu'à ce que la sortie casse — la
`N`-ième transformation appliquée, dans l'ordre du programme et toutes
passes confondues, est celle qu'il faut examiner. Combinez-la avec `-d`
pour que la coupable soit nommée directement dans le fichier de sortie,
et avec `-v` pour obtenir un compte rendu final du nombre de
transformations utilisées.

---

## Publication (estampillage de la version)

La version se trouve à un seul endroit : le `#define` `VERSION` de
`inc/v32opt.h` (le même schéma `AAAAMMJJ-état` que `v32lua` et `v32c++`,
p. ex. `20261002-dev`, `20261015-release`). `v32opt --version` l'affiche,
la compilation CMake la lit à la configuration (pour le nom des paquets),
et `make version` l'inscrit dans les pages de manuel :

```bash
# après avoir modifié le #define à la main :
$ make version

# ou laissez make modifier aussi inc/v32opt.h :
$ make version VERSION=20261015-release

# puis recompilez, pour que le binaire affiche la nouvelle version
$ make
```

`make version` affiche la version et les lignes qu'il tient à jour : le
`#define` de l'en-tête et la ligne `.TH` de chaque page de manuel
(anglais, espagnol et français), qui reçoit aussi le mois et l'année en
cours, dans la langue de la page.
