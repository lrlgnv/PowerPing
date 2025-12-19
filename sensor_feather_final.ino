#include <Adafruit_SleepyDog.h>
#include <RH_RF69.h>
#include <SPI.h>

const int sensor_id = 1;
const float v_warning = 3.60;
const float v_distress = 3.55;

int sensorPin = A0;
int sensorValue = 0;
const float conversion_factor = 3.3f / (1<<10);
int distress = 0;

#define RF69_FREQ 915.0
#define RFM69_CS      8
#define RFM69_INT     3
#define RFM69_RST     4
#define LED          13
RH_RF69 rf69(RFM69_CS, RFM69_INT);

void setup() {
  pinMode(RFM69HCW_CS, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  Serial.begin(9600);

  if (!rf69.init()) {
    /* If we can't initialize the radio, stop here. */
    Serial.println("rf69.init() failed");
    for (;;)
      ;
  }

  if (!rf69.setFrequency(915.0)) {
    Serial.println("rf69.setFrequency() failed");
    for (;;)
      ;
  }

  rf69.setTxPower(20, true);

  /* The encryption keys must match between sensor and receiver. */
  uint8_t key[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
  rf69.setEncryptionKey(key);
}

void loop() {   
  float voltage = analogRead(VBATPIN);
  voltage *= 2;    // we divided by 2, so multiply back
  voltage *= 3.3;  // Multiply by 3.3V, our reference voltage
  voltage /= 1024; // convert to voltage

  if (voltage < v_distress)
    distress = 1;

  /* Construct a ten-byte status update message for the receiver. */
  uint8_t data[10] = "PoPi"; // magic number for PowerPing packets
  if (voltage < v_warning)
    data[4] = distress? 'D' : 'W';
  else
    data[4] = 'U';  data[5] = (uint8_t) sensor_id;
  data[6] = ((char *)(&voltage))[0]; // the reported
  data[7] = ((char *)(&voltage))[1]; // voltage, one
  data[8] = ((char *)(&voltage))[2]; // byte at
  data[9] = ((char *)(&voltage))[3]; // a time.

  rf69.send(data, sizeof data);
  rf69.waitPacketSent();

  uint8_t buf[RH_RF69_MAX_MESSAGE_LEN];
  uint8_t len = sizeof buf;

int i = 0;
  while (i < 3) {
    if (rf69.waitAvailableTimeout(500)) {
      if (rf69.recv(buf, &len)) {
        if (buf[0] == 'P' && buf[1] == 'o' && buf[2] == 'P' && buf[3] == 'i' &&
            buf[4] == 'A' && buf[5] == (uint8_t) sensor_id) {
          /* The acknowledgement was just received, so we're done for now. */
          i = 4;
        }
        else {
          /* Wait a random amount of time before listening again. */
          delay(100 + random(200));
        }
      }
      else {
        Serial.println("No ACK, trying again.");
        i++;
      }
    }
    else {
      Serial.println("No ACK, trying again.");
      i++;
    }

    if (i==3){
      Serial.println("Failed after three attempts.");
    }
  }

  /* Force the radio into a low-power state. RadioHead will wake it back up when we use it again. */
  rf69.sleep();

  if (distress)
    Watchdog.sleep(); // This should never return.

  /* Sleep for about an hour. */
  Watchdog.sleep(3600000 + random(200));
}