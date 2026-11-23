# Flasher l’ATtiny45 avec un ESP32 et Arduino IDE

## Pourquoi on a changé de programmateur

Avec l’USBasp, l’ATtiny45 répondait avec la signature `1E 92 06`. Le firmware de 368 octets avait été écrit et vérifié. Après l’écriture de **LFUSE = `0xE4`**, la cible ne répondait plus à cet adaptateur, même après plusieurs essais de ralentissement.

`0xE4` sélectionne l’oscillateur **interne 128 kHz** sans division par 8. Ce n’est pas une sélection d’horloge externe. La cible tourne alors beaucoup plus lentement : il faut aussi ralentir l’horloge ISP. Les essais historiques n’établissent pas à eux seuls la cause exacte de la panne USBasp. Le sketch ESP32 retrouvé fixe directement les demi-périodes SCK à 50 µs, soit environ 10 kHz, au lieu de dépendre du réglage `-B` de l’adaptateur.

La fiche [Microchip ATtiny25/45/85](https://ww1.microchip.com/downloads/en/DeviceDoc/atmel-2586-avr-8-bit-microcontroller-attiny25-attiny45-attiny85_datasheet.pdf), section Serial Programming, impose un SCK compatible avec l’horloge de la cible. À 128 kHz, une horloge ISP de 10 kHz laisse une marge par rapport à la limite usuelle d’un quart de la fréquence CPU.

## 1. Préparer le matériel

- Un ESP32-WROOM-32 sur carte de développement, connecté au Mac/PC par USB.
- Le PCB avec **ATtiny45**. La procédure ne doit pas être appliquée telle quelle à un autre MCU.
- Six connexions : MOSI, MISO, SCK, RESET, 3,3 V et GND.
- Arduino IDE avec le support des cartes ESP32 et AVRDUDE sur l’ordinateur.

Programmer la carte **déconnectée de la N64**, pour ne pas piloter le CPU ou le circuit RESET de la console par les lignes ISP. Utiliser une alimentation cible 3,3 V et une masse commune. Si la broche 3V3 du DevKit alimente ce petit PCB seul, ne pas connecter en parallèle une autre alimentation. Ne jamais envoyer du 5 V vers les GPIO ESP32.

## 2. Charger le sketch dans l’ESP32

1. Ouvrir `programming/ESP32_AVR_ISP/ESP32_AVR_ISP.ino` dans Arduino IDE.
2. Sélectionner la carte correspondant à l’ESP32-WROOM-32, par exemple **ESP32 Dev Module**, et son port USB.
3. Vérifier les constantes : MOSI 23, MISO 19, SCK 18, RESET 22, `BAUDRATE 19200`, `SPI_HALF_PERIOD_US 50`.
4. Téléverser normalement le sketch dans **l’ESP32**, avec le bouton de téléversement Arduino IDE.
5. Fermer le moniteur série avant d’utiliser AVRDUDE. Laisser le sketch tourner. Ne pas laisser GPIO22 maintenir l’ESP32 lui-même en reset : il doit commander le RESET de l’ATtiny.

Le réglage de vitesse de téléversement Arduino IDE concerne l’ESP32. Les **19 200 bauds** utilisés ci-dessous concernent ensuite la liaison entre AVRDUDE et le sketch.

## 3. Câbler ESP32 → PCB overclock

Couper les alimentations avant le câblage. Les numéros ci-dessous sont des **GPIO**, pas des positions physiques sur le connecteur ESP32.

| ESP32-WROOM-32 | Pastille du PCB | Broche physique ATtiny45 SOIC-8 |
|---|---|---|
| **GPIO23** | `5/MOSI` | 5, PB0 |
| **GPIO19** | `6/MISO` | 6, PB1 |
| **GPIO18** | `7/SCK` | 7, PB2 |
| **GPIO22** | `1/RST` | 1, PB5/RESET |
| **GND** | `4/GND` ou J6 GND | 4 |
| **3V3 régulé** | `TP6`, marqué `8/3.3V`, ou J1 3,3 V | 8 |

Les nombres gravés `1`, `4`, `5`, `6`, `7`, `8` désignent les broches de l’ATtiny. **Ne pas les confondre avec des GPIO ESP32 du même numéro.** Vérifier la broche 1 avec le détrompeur du boîtier et la sérigraphie, pas avec l’orientation de la photo.

## 4. Récupérer et compiler le firmware ManCloud

Les sources locales retrouvées correspondent à la révision `e2d35d55d54be86a5c34685d3ce0b6f555409846` de [ManCloud/n64_overclock](https://github.com/ManCloud/n64_overclock/tree/e2d35d55d54be86a5c34685d3ce0b6f555409846). Le source est GPL-2.0-only. Avec Git, make et les outils AVR (`avr-gcc`, `avr-objcopy`, `avr-size`) disponibles :

```sh
git clone https://github.com/ManCloud/n64_overclock.git
cd n64_overclock
git checkout e2d35d55d54be86a5c34685d3ce0b6f555409846
make tiny45_CC
```

Le résultat attendu est `n64_overclock_t45_2_0X_CC.hex`, pour une LED à **cathode commune** et les modes jusqu’à ×2. La reconstruction de cette variante lors de la préparation du dépôt donne **368 octets de flash**, comme dans le compte rendu historique.

**Ne pas lancer `make fuse` de cette révision pour l’ATtiny45.** Cette cible contient des valeurs `0x7B/0xFE` correspondant à un autre MCU et force la signature avec `-F`. Les valeurs ATtiny45 documentées par ManCloud sont `LFUSE=0xE4`, `HFUSE=0x5F`. Les commandes séparées ci-dessous évitent ce piège.

## 5. Choisir le port et lire la cible

Sur macOS, repérer le port de l’ESP32 :

```sh
ls /dev/cu.*
```

Définir `PORT` avec le vrai port affiché. Exemple à remplacer :

```sh
PORT=/dev/cu.usbserial-XXXX
avrdude -c stk500v1 -p t45 -P "$PORT" -b 19200 -v
```

Attendre la signature **`0x1e9206`**. Puis lire les fusibles et sauvegarder l’état existant avant un effacement :

```sh
avrdude -c stk500v1 -p t45 -P "$PORT" -b 19200 \
  -U lfuse:r:-:h -U hfuse:r:-:h -U efuse:r:-:h
avrdude -c stk500v1 -p t45 -P "$PORT" -b 19200 \
  -U flash:r:attiny45-before.hex:i -U eeprom:r:attiny45-before.eep:i
```

Sur notre première carte, les lectures initiales étaient `LFUSE=0x62`, `HFUSE=0xDF`, `EFUSE=0xFF`. Ce sont des observations historiques, pas des valeurs à écrire automatiquement sur chaque puce.

## 6. Écrire et vérifier la flash, avant les fusibles

Depuis le dossier où se trouve le HEX compilé :

```sh
avrdude -c stk500v1 -p t45 -P "$PORT" -b 19200 \
  -U flash:w:n64_overclock_t45_2_0X_CC.hex:i
avrdude -c stk500v1 -p t45 -P "$PORT" -b 19200 \
  -U flash:v:n64_overclock_t45_2_0X_CC.hex:i
```

L’écriture de la flash peut effacer l’EEPROM selon la configuration d’effacement. Faire les sauvegardes précédentes si la carte a déjà été utilisée. Attendre la réussite de l’écriture et de la vérification. Ne pas employer `-F` pour ignorer une mauvaise signature, ni désactiver la vérification pour masquer une erreur.

## 7. Passer l’ATtiny à 128 kHz, puis contrôler

```sh
avrdude -c stk500v1 -p t45 -P "$PORT" -b 19200 -U lfuse:w:0xE4:m
avrdude -c stk500v1 -p t45 -P "$PORT" -b 19200 \
  -U lfuse:r:-:h -U hfuse:r:-:h
avrdude -c stk500v1 -p t45 -P "$PORT" -b 19200 \
  -U flash:v:n64_overclock_t45_2_0X_CC.hex:i
```

Le sketch garde son SPI logiciel lent après ce changement. Le paramètre AVRDUDE `-B` n’est pas ce qui fixe cette vitesse : c’est `SPI_HALF_PERIOD_US` dans le sketch. Pour un SCK encore plus lent, augmenter cette valeur puis retéléverser le sketch dans l’ESP32. Si la signature disparaît à cette étape, résoudre le problème **avant HFUSE**.

## 8. HFUSE en toute dernière étape

Le firmware utilise PB5 comme entrée du bouton RESET. Pour cela, ManCloud demande **HFUSE=`0x5F`**, qui programme `RSTDISBL`.

**Après cette opération, RESET n’est plus une entrée de programmation et l’ISP classique devient indisponible. L’ESP32 lent ne peut pas rétablir RESET. Pour revenir en arrière, il faut une procédure HVSP adaptée à l’ATtiny45.** Ne pas appliquer manuellement du 12 V à la carte comme tentative de récupération.

Uniquement après validation des étapes précédentes, terminer par :

```sh
avrdude -c stk500v1 -p t45 -P "$PORT" -b 19200 -U hfuse:w:0x5F:m
```

Ne pas prévoir une nouvelle session ISP après sortie du mode programmation : son échec peut être normal avec RESET désactivé. Conserver le journal de la dernière écriture. En cas d’erreur pendant cette écriture, ne pas conclure à sa réussite sans diagnostic.

Couper l’alimentation, débrancher l’ESP32, puis installer la carte dans la N64 selon le câblage adapté à sa révision. Ne pas tenter de communiquer en ISP avec la console raccordée.

## Dépannage rapide

| Symptôme | À vérifier |
|---|---|
| `stk500_getsync` ou timeout | Port correct, moniteur série fermé, sketch lancé, protocole `stk500v1`, 19200 bauds |
| Signature `000000` ou `ffffff` | Alimentation 3,3 V, masse commune, RESET cible, MOSI/MISO/SCK, sens du SOIC-8 |
| USBasp muet après LFUSE `0xE4` | SCK réellement assez lent ; utiliser le sketch ESP32 plutôt que supposer que `-B` est pris en charge |
| ISP inaccessible après HFUSE `0x5F` | Conséquence de `RSTDISBL`, pas un problème que la réduction de SCK résout |
| Bouton RESET mal intercepté | Isolation entre côté bouton et côté PIF ; RIN et ROUT ne doivent pas être court-circuités |

## Ce qui a été retrouvé et ce qui a été vérifié

Le compte rendu du 28 octobre documente le flash USBasp et l’incident LFUSE. Le sketch ESP32 a été récupéré dans le fichier Arduino sauvegardé pour le projet, avec les GPIO et les timings ci-dessus. Les commandes ont été reconstituées à partir de ce protocole et de la documentation ManCloud ; ce guide n’est pas un journal exhaustif d’une nouvelle session de flash sur matériel.

Les fichiers KiCad ont été relus pour confirmer les pastilles ISP. Aucun fusible et aucune flash d’un appareil réel n’ont été modifiés lors de la préparation de ce dépôt.
