/*********************************************************************
 This is an example for our nRF52 based Bluefruit LE modules

 Pick one up today in the adafruit shop!

 Adafruit invests time and resources providing this open source code,
 please support Adafruit and open-source hardware by purchasing
 products from Adafruit!

 MIT license, check LICENSE for more information
 All text above, and the splash screen below must be included in
 any redistribution
*********************************************************************/
#include <bluefruit.h>

#define DIVIDER A0
#define PULLUP_A0 A1

// 00453ed2-f5e0-11ea-b224-00155df38b93
uint8_t batterySvcUuidBytes[16] = 
{ 
  0x93, 0x8b, 0xf3, 0x5d, 0x15, 0x00, 0x24, 0xb2, 
  0xea, 0x11, 0xe0, 0xF5, 0xd2, 0x3e, 0x45, 0x00, 
};

// 00453ed2-f5e0-11ea-b224-00155df38b92
uint8_t batteryUuidBytes[16] = 
{ 
  0x92, 0x8b, 0xf3, 0x5d, 0x15, 0x00, 0x24, 0xb2, 
  0xea, 0x11, 0xe0, 0xF5, 0xd2, 0x3e, 0x45, 0x00, 
};

BLEUuid batteryUuid = BLEUuid(batteryUuidBytes);
BLEUuid batterySvcUuid = BLEUuid(batterySvcUuidBytes);

/* Health Thermometer Service Definitions
 * Health Thermometer Service:  0x1809
 * Temperature Measurement Char: 0x2A1C
 */
BLEService        htms = BLEService(batterySvcUuid);
BLECharacteristic htmc = BLECharacteristic(batteryUuid);

SoftwareTimer adcSample;

BLEDis bledis;    // DIS (Device Information Service) helper class instance

uint16_t adcvalue = 0;

// Advanced function prototypes
void startAdv(void);
void setupHTM(void);

void setup() {
  //Serial.begin(115200);
  // Initialise the Bluefruit module
  Bluefruit.begin();
  Bluefruit.setName("Test52");
  Bluefruit.setTxPower(4);

  // request a slower connection update interval
  Bluefruit.Periph.setConnSupervisionTimeoutMS(5000);
  Bluefruit.Periph.setConnIntervalMS(400, 1000);

  // Blue LED off for lowest power consumption
  Bluefruit.autoConnLed(false);

  // Configure and Start the Device Information Service
  bledis.setManufacturer("Testing");
  bledis.setModel("Test nrf52823");
  bledis.begin();

  // setup bluetooth service/characteristic
  setupHTM();

  // Setup the advertising packet(s)
  startAdv();

  // setup ADC
  pinMode(PULLUP_A0, OUTPUT);
  digitalWrite(PULLUP_A0, LOW);
  analogReference(AR_INTERNAL_1_8);
  analogReadResolution(12); // 8, 10, 12, or 14(oversampling)

  // instead of constant latency
  sd_power_mode_set(NRF_POWER_MODE_LOWPWR);

  // call adcSampleCallback every 1000ms
  adcSample.begin(1000, adcSampleCallback);
  adcSample.start();
}

void startAdv(void)
{
  // Advertising packet
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();

  // Include HTM Service UUID
  Bluefruit.Advertising.addService(htms);

  // Include Name
  Bluefruit.Advertising.addName();
  
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setIntervalMS(100, 1000);
  Bluefruit.Advertising.setFastTimeout(30);      // number of seconds in fast mode
  Bluefruit.Advertising.start(0);                // 0 = Don't stop advertising after n seconds  
}

void setupHTM(void)
{
  htms.begin();
  htmc.setProperties(CHR_PROPS_INDICATE|CHR_PROPS_READ);
  htmc.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
  htmc.setFixedLen(2);
  htmc.setCccdWriteCallback(cccd_callback);  // Optionally capture CCCD updates
  htmc.begin();
  htmc.write(&adcvalue, sizeof(adcvalue)); // Use .write for init data
}

void cccd_callback(uint16_t conn_hdl, BLECharacteristic* chr, uint16_t cccd_value) {
}

uint16_t readAdc() {
  uint16_t value;

  // pull resistor to VCC
  digitalWrite(PULLUP_A0, HIGH);
  // wait for capacitor to charge, 90kohms into 4.7nF should be around 0.4ms
  delay(5);
  value = analogRead(DIVIDER);
  // turn off current
  digitalWrite(PULLUP_A0, LOW);

  return value;
}

void loop() {
  delay(60000);
}

void adcSampleCallback(TimerHandle_t xTimerID) {
  (void) xTimerID; // unused

  if ( Bluefruit.connected() ) {
    adcvalue = readAdc();
    //Serial.println(adcvalue);
    // Note: We use .indicate instead of .write!
    // If it is connected but CCCD is not enabled
    // The characteristic's value is still updated although indicate is not sent
    htmc.indicate(&adcvalue, sizeof(adcvalue));
  }
}
