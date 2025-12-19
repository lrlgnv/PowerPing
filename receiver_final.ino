#include <SPI.h>
#include <RH_RF69.h>

#define NUM_SENSORS 255

struct reading {
  float voltage;
  uint8_t last_rssi;
  char status;
};

struct reading sensor_readings[NUM_SENSORS] = {0};
int audit = 0;

#define RF69_FREQ 915.0
#define RFM69_CS      8
#define RFM69_INT     3
#define RFM69_RST     4
#define LED          13
RH_RF69 rf69(RFM69_CS, RFM69_INT);

void setup() {
  Serial.begin(115200);

  pinMode(LED, OUTPUT);
  pinMode(RFM69_RST, OUTPUT);

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
  /* Listen for incoming messages from the sensors. */
  if (rf69.available()) {
    uint8_t buf[RH_RF69_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);
    if (rf69.recv(buf, &len)) {
      uint8_t rssi = rf69.lastRssi();
      if (!len) return;
      buf[len] = 0;
      if (audit)
        /* In audit mode, echo every incoming message to UART in a human-readable format. */
        Serial.printf("sensor %d: %c %fV, %ddB\r\n", buf[5], buf[4], *(float *) &buf[6], rssi / 2);
      if (buf[0] == 'P' && buf[1] == 'o' && buf[2] == 'P' && buf[3] == 'i' && (buf[4] == 'U' || buf[4] == 'D' || buf[4] == 'W')) {
        /* Acknowledge the message */
        uint8_t data[] = "PoPiA ";
        data[5] = buf[5];
        rf69.send(data, sizeof(data));
        rf69.waitPacketSent();
        sensor_readings[buf[5]].voltage = *(float *) &buf[6];
        sensor_readings[buf[5]].last_rssi = rssi / 2;
        sensor_readings[buf[5]].status = buf[4] == 'U'? 'N' : buf[4];
      }
      /* If the "PoPi" header is missing, ignore the message. (Old protocol, no longer used.) */
    }
  }

  /* Listen for incoming commands from the computer via UART. */
  if (Serial.available()) {
    char usrcmd = Serial.read();
    if (audit) {
      /* One exits audit mode by issuing the X command. */
      if (usrcmd == 'X')
        audit = 0;
    }
    else {
      /* One engages audit mode by issuing the A command. */
      if (usrcmd == 'A')
        audit = 1;
      /* In normal mode, the receiver reports any new sensor updates it has in response to the R command. */
      else if (usrcmd == 'R')
        for (int i = 0; i < NUM_SENSORS; ++i) {
          if (sensor_readings[i].voltage != 0) {
            Serial.printf("%d %f %d %c\r\n", i, sensor_readings[i].voltage, 
                          sensor_readings[i].last_rssi, sensor_readings[i].status);
        }
      }
    }
  }
}