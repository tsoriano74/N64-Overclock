/*
  ESP32 AVR ISP Programmer
  Adaptation ArduinoISP pour ESP32-WROOM-32

  Cible : ATtiny45 / ATtiny85
  Protocole : STK500v1
  Série : 19200 bauds
  SPI : logiciel, environ 10 kHz

  Connexions :
  ESP32 GPIO23 -> ATtiny MOSI (PB0)
  ESP32 GPIO19 <- ATtiny MISO (PB1)
  ESP32 GPIO18 -> ATtiny SCK  (PB2)
  ESP32 GPIO22 -> ATtiny RESET (PB5)
  ESP32 GND    -> ATtiny GND

  Alimentation cible : 3,3 V régulés.
  Ne jamais appliquer 5 V aux GPIO de l'ESP32.
*/

#include <Arduino.h>

// ============================================================
// CONFIGURATION
// ============================================================

#define PIN_MOSI  23
#define PIN_MISO  19
#define PIN_SCK   18
#define PIN_RESET 22

#define BAUDRATE 19200

// SPI logiciel : deux demi-périodes d'au moins 50 us.
// Fréquence effective <= 10 kHz.
#define SPI_HALF_PERIOD_US 50

// ============================================================
// PROTOCOLE STK500v1
// ============================================================

#define STK_OK      0x10
#define STK_FAILED  0x11
#define STK_UNKNOWN 0x12
#define STK_INSYNC  0x14
#define STK_NOSYNC  0x15
#define CRC_EOP     0x20

#define HWVER 2
#define SWMAJ 1
#define SWMIN 18

// ============================================================
// VARIABLES
// ============================================================

bool pmode = false;
bool rst_active_high = false;

uint16_t here = 0;
uint8_t buff[256];

struct Parameter {
  uint8_t devicecode;
  uint8_t revision;
  uint8_t progtype;
  uint8_t parmode;
  uint8_t polling;
  uint8_t selftimed;
  uint8_t lockbytes;
  uint8_t fusebytes;
  uint8_t flashpoll;

  uint16_t eeprompoll;
  uint16_t pagesize;
  uint16_t eepromsize;

  uint32_t flashsize;
};

Parameter param;

// ============================================================
// SPI LOGICIEL LENT
// ============================================================

void spiBegin() {

  digitalWrite(PIN_SCK, LOW);
  digitalWrite(PIN_MOSI, LOW);

  pinMode(PIN_SCK, OUTPUT);
  pinMode(PIN_MOSI, OUTPUT);
  pinMode(PIN_MISO, INPUT);

}

void spiEnd() {

  digitalWrite(PIN_SCK, LOW);

  pinMode(PIN_SCK, INPUT);
  pinMode(PIN_MOSI, INPUT);
  pinMode(PIN_MISO, INPUT);

}

uint8_t spiTransfer(uint8_t data) {

  uint8_t received = 0;

  for (uint8_t i = 0; i < 8; i++) {

    // Préparer MOSI pendant SCK bas
    digitalWrite(PIN_MOSI, (data & 0x80) ? HIGH : LOW);

    delayMicroseconds(SPI_HALF_PERIOD_US);

    // Front montant
    digitalWrite(PIN_SCK, HIGH);

    delayMicroseconds(SPI_HALF_PERIOD_US);

    // Lire MISO
    received <<= 1;

    if (digitalRead(PIN_MISO)) {
      received |= 1;
    }

    // Front descendant
    digitalWrite(PIN_SCK, LOW);

    data <<= 1;

  }

  return received;
}

uint8_t spiTransaction(
  uint8_t a,
  uint8_t b,
  uint8_t c,
  uint8_t d
) {

  spiTransfer(a);
  spiTransfer(b);
  spiTransfer(c);

  return spiTransfer(d);
}

// ============================================================
// RESET CIBLE
// ============================================================

void resetTarget(bool active) {

  bool level = rst_active_high ? active : !active;

  digitalWrite(PIN_RESET, level ? HIGH : LOW);

}

// ============================================================
// COMMUNICATION SÉRIE
// ============================================================

uint8_t getch() {

  while (!Serial.available()) {
    delay(1);
  }

  return (uint8_t)Serial.read();
}

void fill(uint16_t n) {

  for (uint16_t i = 0; i < n; i++) {
    buff[i] = getch();
  }

}

void sendByte(uint8_t value) {
  Serial.write(value);
}

void emptyReply() {

  if (getch() == CRC_EOP) {

    sendByte(STK_INSYNC);
    sendByte(STK_OK);

  } else {

    sendByte(STK_NOSYNC);

  }

}

void byteReply(uint8_t value) {

  if (getch() == CRC_EOP) {

    sendByte(STK_INSYNC);
    sendByte(value);
    sendByte(STK_OK);

  } else {

    sendByte(STK_NOSYNC);

  }

}

// ============================================================
// PARAMÈTRES
// ============================================================

uint16_t read16BE(uint8_t *p) {

  return ((uint16_t)p[0] << 8) | p[1];

}

void setParameters() {

  param.devicecode = buff[0];
  param.revision = buff[1];
  param.progtype = buff[2];
  param.parmode = buff[3];
  param.polling = buff[4];
  param.selftimed = buff[5];
  param.lockbytes = buff[6];
  param.fusebytes = buff[7];
  param.flashpoll = buff[8];

  param.eeprompoll = read16BE(&buff[10]);
  param.pagesize = read16BE(&buff[12]);
  param.eepromsize = read16BE(&buff[14]);

  param.flashsize =
      ((uint32_t)buff[16] << 24)
    | ((uint32_t)buff[17] << 16)
    | ((uint32_t)buff[18] << 8)
    | buff[19];

  rst_active_high = (param.devicecode >= 0xE0);

}

// ============================================================
// MODE PROGRAMMATION
// ============================================================

void startPmode() {

  if (pmode) {
    return;
  }

  // Préparer RESET à l'état inactif
  resetTarget(false);
  pinMode(PIN_RESET, OUTPUT);

  // Activer RESET
  resetTarget(true);

  delay(20);

  spiBegin();

  delay(20);

  // Impulsion RESET pour synchroniser la cible
  resetTarget(false);
  delayMicroseconds(200);
  resetTarget(true);

  delay(50);

  // Commande AVR Programming Enable
  spiTransaction(0xAC, 0x53, 0x00, 0x00);

  pmode = true;

}

void endPmode() {

  spiEnd();

  resetTarget(false);

  pinMode(PIN_RESET, INPUT);

  pmode = false;

}

// ============================================================
// VERSION PROGRAMMATEUR
// ============================================================

void getVersion(uint8_t command) {

  switch (command) {

    case 0x80:
      byteReply(HWVER);
      break;

    case 0x81:
      byteReply(SWMAJ);
      break;

    case 0x82:
      byteReply(SWMIN);
      break;

    case 0x93:
      byteReply('S');
      break;

    default:
      byteReply(0);
      break;

  }

}

// ============================================================
// COMMANDES SPI UNIVERSELLES
// ============================================================

void universal() {

  fill(4);

  uint8_t result = spiTransaction(
    buff[0],
    buff[1],
    buff[2],
    buff[3]
  );

  byteReply(result);

}

// ============================================================
// MÉMOIRE FLASH
// ============================================================

void flashWrite(
  uint8_t hilo,
  uint16_t address,
  uint8_t data
) {

  spiTransaction(
    0x40 + 8 * hilo,
    (address >> 8) & 0xFF,
    address & 0xFF,
    data
  );

}

void commitPage(uint16_t address) {

  spiTransaction(
    0x4C,
    (address >> 8) & 0xFF,
    address & 0xFF,
    0x00
  );

  delay(10);

}

uint16_t currentPage() {

  switch (param.pagesize) {

    case 32:
      return here & 0xFFF0;

    case 64:
      return here & 0xFFE0;

    case 128:
      return here & 0xFFC0;

    case 256:
      return here & 0xFF80;

    default:
      return here;

  }

}

uint8_t writeFlashPages(uint16_t length) {

  uint16_t index = 0;
  uint16_t page = currentPage();

  while (index < length) {

    uint16_t nextPage = currentPage();

    if (page != nextPage) {

      commitPage(page);
      page = nextPage;

    }

    flashWrite(0, here, buff[index++]);
    flashWrite(1, here, buff[index++]);

    here++;

  }

  commitPage(page);

  return STK_OK;

}

void writeFlash(uint16_t length) {

  if (length > sizeof(buff) || (length & 1)) {
    // Consommer les données même en cas d'erreur
    for (uint16_t i = 0; i < length; i++) {
      getch();
    }

    if (getch() == CRC_EOP) {
      sendByte(STK_INSYNC);
      sendByte(STK_FAILED);
    } else {
      sendByte(STK_NOSYNC);
    }

    return;
  }

  fill(length);

  if (getch() == CRC_EOP) {

    sendByte(STK_INSYNC);
    sendByte(writeFlashPages(length));

  } else {

    sendByte(STK_NOSYNC);

  }

}

// ============================================================
// EEPROM
// ============================================================

uint8_t writeEEPROM(uint16_t length) {

  uint32_t start = (uint32_t)here * 2;

  if (length > param.eepromsize ||
      start + length > param.eepromsize ||
      length > sizeof(buff)) {

    for (uint16_t i = 0; i < length; i++) {
      getch();
    }

    return STK_FAILED;
  }

  fill(length);

  for (uint16_t i = 0; i < length; i++) {

    uint16_t address = start + i;

    spiTransaction(
      0xC0,
      (address >> 8) & 0xFF,
      address & 0xFF,
      buff[i]
    );

    delay(10);

  }

  return STK_OK;

}

// ============================================================
// PROGRAM PAGE
// ============================================================

void programPage() {

  uint16_t length = (uint16_t)getch() << 8;
  length |= getch();

  uint8_t memtype = getch();

  if (memtype == 'F') {

    writeFlash(length);
    return;

  }

  if (memtype == 'E') {

    // writeEEPROM consomme les données
    uint8_t result = writeEEPROM(length);

    if (getch() == CRC_EOP) {

      sendByte(STK_INSYNC);
      sendByte(result);

    } else {

      sendByte(STK_NOSYNC);

    }

    return;

  }

  // Type mémoire inconnu
  for (uint16_t i = 0; i < length; i++) {
    getch();
  }

  if (getch() == CRC_EOP) {
    sendByte(STK_INSYNC);
    sendByte(STK_FAILED);
  } else {
    sendByte(STK_NOSYNC);
  }

}

// ============================================================
// LECTURE FLASH
// ============================================================

uint8_t flashRead(
  uint8_t hilo,
  uint16_t address
) {

  return spiTransaction(
    0x20 + 8 * hilo,
    (address >> 8) & 0xFF,
    address & 0xFF,
    0x00
  );

}

uint8_t flashReadPage(uint16_t length) {

  for (uint16_t i = 0; i < length; i += 2) {

    sendByte(flashRead(0, here));
    sendByte(flashRead(1, here));

    here++;

  }

  return STK_OK;

}

// ============================================================
// LECTURE EEPROM
// ============================================================

uint8_t eepromReadPage(uint16_t length) {

  uint32_t start = (uint32_t)here * 2;

  for (uint16_t i = 0; i < length; i++) {

    uint16_t address = start + i;

    uint8_t value = spiTransaction(
      0xA0,
      (address >> 8) & 0xFF,
      address & 0xFF,
      0xFF
    );

    sendByte(value);

  }

  return STK_OK;

}

// ============================================================
// READ PAGE
// ============================================================

void readPage() {

  uint16_t length = (uint16_t)getch() << 8;
  length |= getch();

  uint8_t memtype = getch();

  if (getch() != CRC_EOP) {

    sendByte(STK_NOSYNC);
    return;

  }

  sendByte(STK_INSYNC);

  uint8_t result = STK_FAILED;

  if (memtype == 'F' && !(length & 1)) {
    result = flashReadPage(length);
  }

  if (memtype == 'E') {
    result = eepromReadPage(length);
  }

  sendByte(result);

}

// ============================================================
// SIGNATURE ATtiny
// ============================================================

void readSignature() {

  if (getch() != CRC_EOP) {

    sendByte(STK_NOSYNC);
    return;

  }

  sendByte(STK_INSYNC);

  sendByte(spiTransaction(0x30, 0x00, 0x00, 0x00));
  sendByte(spiTransaction(0x30, 0x00, 0x01, 0x00));
  sendByte(spiTransaction(0x30, 0x00, 0x02, 0x00));

  sendByte(STK_OK);

}

// ============================================================
// INTERPRÉTEUR STK500
// ============================================================

void avrisp() {

  uint8_t command = getch();

  switch (command) {

    case '0':
      emptyReply();
      break;

    case '1':

      if (getch() == CRC_EOP) {

        sendByte(STK_INSYNC);
        Serial.print("AVR ISP");
        sendByte(STK_OK);

      } else {

        sendByte(STK_NOSYNC);

      }

      break;

    case 'A':
      getVersion(getch());
      break;

    case 'B':
      fill(20);
      setParameters();
      emptyReply();
      break;

    case 'E':
      fill(5);
      emptyReply();
      break;

    case 'P':
      startPmode();
      emptyReply();
      break;

    case 'U':
      here = getch();
      here |= (uint16_t)getch() << 8;
      emptyReply();
      break;

    case 0x60:
      getch();
      getch();
      emptyReply();
      break;

    case 0x61:
      getch();
      emptyReply();
      break;

    case 0x64:
      programPage();
      break;

    case 0x74:
      readPage();
      break;

    case 'V':
      universal();
      break;

    case 'Q':
      endPmode();
      emptyReply();
      break;

    case 0x75:
      readSignature();
      break;

    case CRC_EOP:
      sendByte(STK_NOSYNC);
      break;

    default:

      if (getch() == CRC_EOP) {
        sendByte(STK_UNKNOWN);
      } else {
        sendByte(STK_NOSYNC);
      }

      break;

  }

}

// ============================================================
// SETUP
// ============================================================

void setup() {

  // Broches dans un état sûr au démarrage
  pinMode(PIN_MOSI, INPUT);
  pinMode(PIN_MISO, INPUT);
  pinMode(PIN_SCK, INPUT);
  pinMode(PIN_RESET, INPUT);

  Serial.begin(BAUDRATE);

}

// ============================================================
// LOOP
// ============================================================

void loop() {

  if (Serial.available()) {
    avrisp();
  }

}