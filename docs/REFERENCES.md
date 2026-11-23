# Provenance et vérifications

Préparation du dépôt : 23 novembre 2026.

## Sources

- Firmware : [ManCloud/n64_overclock, révision e2d35d5](https://github.com/ManCloud/n64_overclock/tree/e2d35d55d54be86a5c34685d3ce0b6f555409846). Le source `n64_overclock.c` porte l’en-tête GPL version 2 uniquement. La cible de compilation utilisée est `tiny45_CC`. Les sources et le HEX ne sont pas redistribués dans ce dépôt : le guide indique comment récupérer cette révision et compiler.
- Fiche composant : [Microchip ATtiny25/45/85](https://ww1.microchip.com/downloads/en/DeviceDoc/atmel-2586-avr-8-bit-microcontroller-attiny25-attiny45-attiny85_datasheet.pdf), sections System Clock, Fuse Bits et Serial Programming.
- PCB cité dans les échanges : [Consoles Unleashed](https://github.com/consolesunleashed/nintendo-64-switchless-overclock-pcb). Les fichiers livrés ici sont ceux du projet KiCad personnel, sans prétendre être identiques à une révision de ce dépôt.
- Projet historique : [borti4938](https://github.com/borti4938/N64_Switchless_Overclock).

## Reconstitution du parcours

Les échanges « Identifier le MCU », « Référence ATtiny overclock N64 », « Réessayer sans problème » et « Branchement Reset N64 Modifiés » ont permis de retrouver le contexte du projet. Un compte rendu local daté du 28 octobre 2026 confirme : signature ATtiny45 `1E 92 06`, firmware de 368 octets écrit et vérifié par USBasp, LFUSE `0xE4` annoncé écrit puis perte de communication, HFUSE encore inchangé à ce stade. La récupération HVSP y était envisagée, mais pas réalisée.

Le sketch sauvegardé `sketch_oct28a.ino` a été copié à l’identique dans `programming/ESP32_AVR_ISP/ESP32_AVR_ISP.ino`. Il définit GPIO23/19/18/22 et une demi-période SPI de 50 µs. La procédure explique la méthode ESP32 indiquée par l’auteur, sans présenter les essais abandonnés de HVSP comme une méthode réussie.

## Points de vigilance

- La BOM et les positions originales sont préservées. La BOM mélange 4 pièces à monter, 6 points de test et 8 pastilles de raccordement. Toutes les références LCSC y sont vides.
- Aucun contrôle ERC/DRC favorable n’est revendiqué ici, et les Gerbers n’ont pas été régénérés. Vérifier leur correspondance avant une nouvelle commande.
- Le PCB relie la broche 6 de l’ATtiny à la pastille CPU112 et la broche 7 à CPU116. Le README amont et certains commentaires du source ManCloud ne décrivent pas ces correspondances de façon uniforme. Les repères du présent dépôt sont extraits du PCB fourni ; vérifier le comportement réel et les modes sur la console au lieu de déduire sa fréquence d’une couleur de LED.
- Le sketch récupéré implémente un programmateur ISP logiciel ; il ne réalise pas une récupération HVSP et ne réactive pas RESET après `RSTDISBL`.
- Les photos montrent une carte assemblée. Elles ne constituent pas une mesure de fréquence CPU, de température ou de stabilité.
- Les modèles 3D et bibliothèques externes KiCad peuvent nécessiter une installation locale. Les empreintes et symboles déjà placés sont intégrés aux fichiers.

Les essais de compilation effectués pendant la préparation sont consignés séparément dans `BUILD-CHECKS.md`.
