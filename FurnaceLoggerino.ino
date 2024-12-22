/*
 * Furnace Logger - Arduino Uno + Adafruit Logger shield
 * https://learn.adafruit.com/adafruit-data-logger-shield
 */

// I/O provided by the Adafruit Logger shield
#define LED_GREEN   (3)
#define LED_RED     (5)
#define SQ_WAVE_IN  (7)
#define WRITE_PROT  (8)
#define CARD_DET    (9)
#define SD_CS       (10)

// Temperature sensor using OneWire interface
#define DS18B20_PIN (2)

// Monitor inputs connected to A0/A1/A2
#define FAN_IN      (A0)
#define LOW_IN      (A1)
#define HIGH_IN     (A2)

#include <SPI.h>
#include <SD.h>
#include <RTClib.h>

Sd2Card card;
SdVolume volume;
SdFile root;
const int chipSelect = SD_CS;
RTC_PCF8523 rtc;

const char *dayOfTheWeek[7] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};

void sd_init() {
  Serial.print("\nInitializing SD card...");

  // we'll use the initialization code from the utility libraries
  // since we're just testing if the card is working!
  if (!card.init(SPI_HALF_SPEED, chipSelect)) {
    Serial.println("initialization failed. Things to check:");
    Serial.println("* is a card inserted?");
    Serial.println("* is your wiring correct?");
    Serial.println("* did you change the chipSelect pin to match your shield or module?");
    Serial.println("Note: press reset button on the board and reopen this Serial Monitor after fixing your issue!");
    while (1);
  } else {
    Serial.println("Wiring is correct and a card is present.");
  }

  // print the type of card
  Serial.println();
  Serial.print("Card type:         ");
  switch (card.type()) {
    case SD_CARD_TYPE_SD1:
      Serial.println("SD1");
      break;
    case SD_CARD_TYPE_SD2:
      Serial.println("SD2");
      break;
    case SD_CARD_TYPE_SDHC:
      Serial.println("SDHC");
      break;
    default:
      Serial.println("Unknown");
  }

  // Now we will try to open the 'volume'/'partition' - it should be FAT16 or FAT32
  if (!volume.init(card)) {
    Serial.println("Could not find FAT16/FAT32 partition.\nMake sure you've formatted the card");
    while (1);
  }

  Serial.print("Clusters:          ");
  Serial.println(volume.clusterCount());
  Serial.print("Blocks x Cluster:  ");
  Serial.println(volume.blocksPerCluster());

  Serial.print("Total Blocks:      ");
  Serial.println(volume.blocksPerCluster() * volume.clusterCount());
  Serial.println();

  // print the type and size of the first FAT-type volume
  uint32_t volumesize;
  Serial.print("Volume type is:    FAT");
  Serial.println(volume.fatType(), DEC);

  volumesize = volume.blocksPerCluster();    // clusters are collections of blocks
  volumesize *= volume.clusterCount();       // we'll have a lot of clusters
  volumesize /= 2;                           // SD card blocks are always 512 bytes (2 blocks are 1 KB)
  Serial.print("Volume size (KB):  ");
  Serial.println(volumesize);
  Serial.print("Volume size (MB):  ");
  volumesize /= 1024;
  Serial.println(volumesize);
  Serial.print("Volume size (GB):  ");
  Serial.println((float)volumesize / 1024.0);

  Serial.println("\nFiles found on the card (name, date and size in bytes): ");
  root.openRoot(volume);

  // list all files in the card with date and size
  root.ls(LS_R | LS_DATE | LS_SIZE);
  root.close();
}

void setup() {
  Serial.begin(115200);

  pinMode(LED_GREEN, OUTPUT);
  digitalWrite(LED_GREEN, LOW);
  pinMode(LED_RED, OUTPUT);
  digitalWrite(LED_RED, LOW);

  pinMode(SQ_WAVE_IN, INPUT_PULLUP);
  pinMode(CARD_DET, INPUT_PULLUP);
  pinMode(WRITE_PROT, INPUT_PULLUP);

  pinMode(FAN_IN, INPUT);
  pinMode(LOW_IN, INPUT);
  pinMode(HIGH_IN, INPUT);

  if (! rtc.begin()) {
    Serial.println("ERROR: RTC not found");
    Serial.flush();
    while(1) delay(10);
  }
  
  if (! rtc.initialized() || rtc.lostPower()) {
    Serial.println("Set RTC:");

    // FIXME:
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  }

  rtc.start();

  sd_init();

}

DateTime last;

void loop() {
  // put your main code here, to run repeatedly:
  DateTime now = rtc.now();

  digitalWrite(LED_RED, digitalRead(CARD_DET));

  if (now != last) {
    last = now;

    digitalWrite(LED_GREEN, (now.second() & 1) ? HIGH : LOW);

    Serial.print(now.year(), DEC);
    Serial.print('/');
    Serial.print(now.month(), DEC);
    Serial.print('/');
    Serial.print(now.day(), DEC);
    Serial.print(" (");
    Serial.print(dayOfTheWeek[now.dayOfTheWeek()]);
    Serial.print(") ");
    Serial.print(now.hour(), DEC);
    Serial.print(':');
    Serial.print(now.minute(), DEC);
    Serial.print(':');
    Serial.print(now.second(), DEC);
    Serial.println();
/*
    Serial.print(" since midnight 1/1/1970 = ");
    Serial.print(now.unixtime());
    Serial.print("s = ");
    Serial.print(now.unixtime() / 86400L);
    Serial.println("d");
*/
  }

  delay(10);

}
