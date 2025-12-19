#include <Adafruit_SleepyDog.h>
#include <RH_RF69.h>
#include <SPI.h>

/* The radio's CS is wired to GPIO 6, and its interrupt pin on GPIO 5. */
#define RF69_FREQ 915.0
RH_RF69 rf69(6, 5);

int sensor_id = 3;
float voltage = 3.3;
int distress = 0;

void setup() 
{
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

void loop()
{
  /* Sending the dummy sensor the 'D' command over UART puts it into distress mode. */
  if (Serial.available()) {
    char usrcmd = Serial.read();

    if (usrcmd = 'D')
      distress = 1;
  }

  /* Dummy sensors always report 3.3V */
  float voltage = 3.3;

  /* Construct a ten-byte status update message for the receiver. */
  uint8_t data[10] = "PoPi"; // magic number for PowerPing packets
  data[4] = distress? 'D' : 'U'; // D for dead, U for update
  data[5] = (uint8_t) sensor_id; // next comes ID of the sensor
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

  /* Sleep for a little while. */
  Watchdog.sleep(500 + random(200));
}