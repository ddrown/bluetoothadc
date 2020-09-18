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
#include <Adafruit_LSM6DSOX.h>
#include <Adafruit_LIS3MDL.h>
#include <elapsedMillis.h>

Adafruit_LIS3MDL lis3mdl;
Adafruit_LSM6DSOX lsm6ds;

#define DIVIDER A0
#define PULLUP_A0 A1

// 00453ed2-f5e0-11ea-b224-00155df38b93
uint8_t dataSvcUuidBytes[16] =
{
  0x93, 0x8b, 0xf3, 0x5d, 0x15, 0x00, 0x24, 0xb2,
  0xea, 0x11, 0xe0, 0xF5, 0xd2, 0x3e, 0x45, 0x00,
};

// 00453ed2-f5e0-11ea-b224-00155df38b92
uint8_t dataUuidBytes[16] =
{
  0x92, 0x8b, 0xf3, 0x5d, 0x15, 0x00, 0x24, 0xb2,
  0xea, 0x11, 0xe0, 0xF5, 0xd2, 0x3e, 0x45, 0x00,
};

BLEService        btservice = BLEService(dataSvcUuidBytes);
BLECharacteristic btdata = BLECharacteristic(dataUuidBytes);

SoftwareTimer adcSample;

BLEDis bledis;    // DIS (Device Information Service) helper class instance

void setup() {
  //Serial.begin(115200);
  // Initialise the Bluefruit module
  Bluefruit.begin();
  Bluefruit.setName("Test52");
  Bluefruit.setTxPower(4);

  lsm6ds.begin_I2C();
  lis3mdl.begin_I2C();

  setupAccelerometer();

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
  setupBTChars();

  // Setup the advertising packet(s)
  startAdv();

  // setup ADC
  setupAdc();

  // instead of constant latency
  sd_power_mode_set(NRF_POWER_MODE_LOWPWR);
}

void setupAdc() {
  pinMode(PULLUP_A0, OUTPUT);
  digitalWrite(PULLUP_A0, LOW);
  analogReference(AR_INTERNAL_1_8);
  analogReadResolution(12); // 8, 10, 12, or 14(oversampling)

  // call adcSampleCallback every 950ms
  adcSample.begin(950, adcSampleCallback);
  adcSample.start();
}

void setupAccelerometer() {
  lsm6ds.setAccelRange(LSM6DS_ACCEL_RANGE_2_G);
  lsm6ds.setAccelDataRate(LSM6DS_RATE_1_6_HZ);
  lsm6ds.setGyroRange(LSM6DS_GYRO_RANGE_125_DPS );
  lsm6ds.setGyroDataRate(LSM6DS_RATE_SHUTDOWN);
  lis3mdl.setDataRate(LIS3MDL_DATARATE_0_625_HZ);
  lis3mdl.setRange(LIS3MDL_RANGE_4_GAUSS);
  lis3mdl.setPerformanceMode(LIS3MDL_LOWPOWERMODE);
  lis3mdl.setOperationMode(LIS3MDL_POWERDOWNMODE);
}

void startAdv(void) {
  // Advertising packet
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();

  // Include Accelerometer Service UUID
  Bluefruit.Advertising.addService(btservice);

  // Include Name
  Bluefruit.Advertising.addName();

  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setIntervalMS(100, 1000);
  Bluefruit.Advertising.setFastTimeout(30);      // number of seconds in fast mode
  Bluefruit.Advertising.start(0);                // 0 = Don't stop advertising after n seconds
}

struct {
  uint16_t battery;
  int16_t x, y, z;
} sensorValues;

void setupBTChars(void) {
  btservice.begin();

  memset(&sensorValues, 0, sizeof(sensorValues));

  btdata.setProperties(CHR_PROPS_INDICATE|CHR_PROPS_READ);
  btdata.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
  btdata.setFixedLen(sizeof(sensorValues));
  btdata.begin();
  btdata.write(&sensorValues, sizeof(sensorValues)); // Use .write for init data
}

void readAdc() {
  sensors_event_t accel;

  // pull resistor to VCC
  digitalWrite(PULLUP_A0, HIGH);
  elapsedMillis chargingDelay = 0;

  lsm6ds.getAccelerometerSensor()->getEvent(&accel);

  sensorValues.x = accel.acceleration.x * 1000;
  sensorValues.y = accel.acceleration.y * 1000;
  sensorValues.z = accel.acceleration.z * 1000;

  // wait for capacitor to charge, 90kohms into 4.7nF should be around 0.4ms
  while (chargingDelay < 5) {
    // do nothing
  }
  sensorValues.battery = analogRead(DIVIDER);
  // turn off current
  digitalWrite(PULLUP_A0, LOW);
}

void loop() {
  delay(60000);
}

void adcSampleCallback(TimerHandle_t xTimerID) {
  (void) xTimerID; // unused

  if ( Bluefruit.connected() ) {
    readAdc();
    // Note: We use .indicate instead of .write!
    // If it is connected but CCCD is not enabled
    // The characteristic's value is still updated although indicate is not sent
    btdata.indicate(&sensorValues, sizeof(sensorValues));
  }
}
