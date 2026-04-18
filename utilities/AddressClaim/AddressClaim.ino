#include <FlexCAN_T4.h>

#define GREEN_LED   6
#define YELLOW_LED  5
#define RED_LED     14
#define BLUE_LED    39

#define SILENT_0    42
#define SILENT_1    41
#define SILENT_2    40

#define BUTTON1_PIN 28
#define BUTTON2_PIN 53

FlexCAN_T4<CAN0, RX_SIZE_256, TX_SIZE_16> Can0;
FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> Can1;

volatile bool greenState  = HIGH;   // CAN0 write
volatile bool yellowState = HIGH;   // CAN0 receive
volatile bool redState    = HIGH;   // CAN1 receive
volatile bool blueState   = HIGH;   // CAN1 write

uint32_t greenLastChangeMs  = 0;
uint32_t yellowLastChangeMs = 0;
uint32_t redLastChangeMs    = 0;
uint32_t blueLastChangeMs   = 0;

const uint32_t ADDRESS_CLAIM_PERIOD_MS = 260;
const uint32_t LED_OFF_TIMEOUT_MS      = 1000;
const uint32_t BUTTON_DEBOUNCE_MS      = 40;

uint8_t currentAddress = 0;
uint32_t lastAddressClaimMs = 0;
bool txEnabled = false;

bool lastButton1Reading = HIGH;
bool lastButton2Reading = HIGH;
bool button1State = HIGH;
bool button2State = HIGH;
uint32_t lastButton1DebounceMs = 0;
uint32_t lastButton2DebounceMs = 0;

static void printCandumpA(const CAN_message_t &msg, const char *ifname) {
  char ascii[9];
  for (uint8_t i = 0; i < 8; i++) ascii[i] = ' ';
  ascii[8] = '\0';

  for (uint8_t i = 0; i < msg.len && i < 8; i++) {
    uint8_t c = msg.buf[i];
    ascii[i] = (c >= 32 && c <= 126) ? (char)c : '.';
  }

  uint32_t sec = millis() / 1000;
  uint32_t usec = micros() % 1000000UL;

  Serial.printf("(%lu.%06lu) %s ", sec, usec, ifname);

  if (msg.flags.extended) {
    Serial.printf("%08lX", msg.id);
  } else {
    Serial.printf("%03lX", msg.id);
  }

  Serial.printf(" [%u]", msg.len);
  for (uint8_t i = 0; i < msg.len; i++) {
    Serial.printf(" %02X", msg.buf[i]);
  }
  Serial.printf("  '%s'\n", ascii);
}

void toggleLed(uint8_t pin, volatile bool &state, uint32_t &lastChangeMs) {
  state = !state;
  digitalWrite(pin, state);
  lastChangeMs = millis();
}

void restoreLedIfOffTooLong(uint8_t pin, volatile bool &state, uint32_t &lastChangeMs) {
  if (state == LOW && (millis() - lastChangeMs >= LED_OFF_TIMEOUT_MS)) {
    state = HIGH;
    digitalWrite(pin, HIGH);
    lastChangeMs = millis();
  }
}

void sendAddressClaim(uint8_t sourceAddress) {
  CAN_message_t msg;
  msg.id = 0x18EEFF00 | sourceAddress;
  msg.len = 8;
  msg.flags.extended = 1;
  msg.flags.remote = 0;

  for (uint8_t i = 0; i < 8; i++) {
    msg.buf[i] = 0x00;
  }

  if (Can0.write(msg)) {
    toggleLed(GREEN_LED, greenState, greenLastChangeMs);
    printCandumpA(msg, "can0 TX");
  }

  if (Can1.write(msg)) {
    toggleLed(BLUE_LED, blueState, blueLastChangeMs);
    printCandumpA(msg, "can1 TX");
  }
}

void can0Callback(const CAN_message_t &msg) {
  toggleLed(YELLOW_LED, yellowState, yellowLastChangeMs);
  printCandumpA(msg, "can0 RX");
}

void can1Callback(const CAN_message_t &msg) {
  toggleLed(RED_LED, redState, redLastChangeMs);
  printCandumpA(msg, "can1 RX");
}

void toggleTransmission() {
  txEnabled = !txEnabled;
  Serial.printf("Address claim transmission %s\n", txEnabled ? "ENABLED" : "DISABLED");
}

void updateButtons() {
  bool reading1 = digitalRead(BUTTON1_PIN);
  bool reading2 = digitalRead(BUTTON2_PIN);

  if (reading1 != lastButton1Reading) lastButton1DebounceMs = millis();
  if (reading2 != lastButton2Reading) lastButton2DebounceMs = millis();

  if ((millis() - lastButton1DebounceMs) > BUTTON_DEBOUNCE_MS) {
    if (reading1 != button1State) {
      button1State = reading1;
      if (button1State == LOW) toggleTransmission();
    }
  }

  if ((millis() - lastButton2DebounceMs) > BUTTON_DEBOUNCE_MS) {
    if (reading2 != button2State) {
      button2State = reading2;
      if (button2State == LOW) toggleTransmission();
    }
  }

  lastButton1Reading = reading1;
  lastButton2Reading = reading2;
}

void setup() {
  Serial.begin(115200);

  pinMode(SILENT_0, OUTPUT);
  pinMode(SILENT_1, OUTPUT);
  pinMode(SILENT_2, OUTPUT);

  // normal mode, not silent
  digitalWrite(SILENT_0, LOW);
  digitalWrite(SILENT_1, LOW);
  digitalWrite(SILENT_2, LOW);

  pinMode(GREEN_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(BLUE_LED, OUTPUT);

  pinMode(BUTTON1_PIN, INPUT_PULLUP);
  pinMode(BUTTON2_PIN, INPUT_PULLUP);

  digitalWrite(GREEN_LED, greenState);
  digitalWrite(YELLOW_LED, yellowState);
  digitalWrite(RED_LED, redState);
  digitalWrite(BLUE_LED, blueState);

  uint32_t now = millis();
  greenLastChangeMs  = now;
  yellowLastChangeMs = now;
  redLastChangeMs    = now;
  blueLastChangeMs   = now;

  Can0.begin();
  Can0.setBaudRate(250000);
  Can0.enableFIFO();
  Can0.enableFIFOInterrupt();
  Can0.onReceive(can0Callback);

  Can1.begin();
  Can1.setBaudRate(250000);
  Can1.enableFIFO();
  Can1.enableFIFOInterrupt();
  Can1.onReceive(can1Callback);

  lastAddressClaimMs = millis();

  Serial.println("System ready. Fixed baud 250000. TX starts DISABLED.");
}

void loop() {
  Can0.events();
  Can1.events();

  updateButtons();

  if (txEnabled && (millis() - lastAddressClaimMs >= ADDRESS_CLAIM_PERIOD_MS)) {
    lastAddressClaimMs += ADDRESS_CLAIM_PERIOD_MS;
    sendAddressClaim(currentAddress);
    currentAddress++;
  }

  restoreLedIfOffTooLong(GREEN_LED, greenState, greenLastChangeMs);
  restoreLedIfOffTooLong(YELLOW_LED, yellowState, yellowLastChangeMs);
  restoreLedIfOffTooLong(RED_LED, redState, redLastChangeMs);
  restoreLedIfOffTooLong(BLUE_LED, blueState, blueLastChangeMs);
}