# Contrôles de compilation

Vérifications du 23 novembre 2026, sans programmation d’un appareil réel.

| Élément | Vérification | Résultat |
|---|---|---|
| Firmware ManCloud, révision `e2d35d55d54be86a5c34685d3ce0b6f555409846` | `make tiny45_CC`, outils AVR présents sur le Mac | Compilation réussie, 368 octets de flash |
| HEX recompilé | Comparaison octet par octet avec le HEX historique retrouvé | Identique |
| Sketch ESP32 récupéré | Compilation Arduino CLI pour `esp32:esp32:esp32`, cœur Espressif 3.3.7 | Réussite, 285896 octets de programme et 22348 octets de variables globales |
| Câblage ISP | Lecture des nets dans le PCB KiCad | Broches 1/4/5/6/7/8 et pastilles confirmées |
| Sources livrées | Comparaison des six fichiers KiCad/production avec les originaux | Identiques |

SHA-256 du HEX ManCloud recompilé : `5202198d0cc390c8c5435ae4b46234013c99869d3f8675c1786f96b0f77e77f4`.

Une compilation réussie ne valide pas à elle seule le flash sur une puce réelle, les fusibles, le comportement de la N64 ou la qualité de fabrication. Le présent contrôle n’a modifié aucun matériel.
