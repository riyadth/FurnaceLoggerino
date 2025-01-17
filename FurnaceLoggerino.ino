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

#define LOGFILE     "FURNACE.LOG"

#include <SPI.h>
#include <SD.h>
#include <RTClib.h>

Sd2Card card;
SdVolume volume;
SdFile root;
const int chipSelect = SD_CS;
RTC_PCF8523 rtc;
DateTime now;
uint32_t boot_time;

// const char *dayOfTheWeek[7] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};

void sd_init() {
  Serial.print(F("\Initialization: "));

  // we'll use the initialization code from the utility libraries
  // since we're just testing if the card is working!
  if (!card.init(SPI_HALF_SPEED, chipSelect)) {
    err(__LINE__, "card.init");
    /*
    Serial.println(F("initialization failed. Things to check:"));
    Serial.println(F("* is a card inserted?"));
    Serial.println(F("* is your wiring correct?"));
    Serial.println(F("* did you change the chipSelect pin to match your shield or module?"));
    Serial.println(F("Note: press reset button on the board and reopen this Serial Monitor after fixing your issue!"));
    */
    while (1);
  } else {
    Serial.println(F("Card present"));
  }

#if 0
  // print the type of card
  Serial.println();
  Serial.print(F("Card type:         "));
  switch (card.type()) {
    case SD_CARD_TYPE_SD1:
      Serial.println(F("SD1"));
      break;
    case SD_CARD_TYPE_SD2:
      Serial.println(F("SD2"));
      break;
    case SD_CARD_TYPE_SDHC:
      Serial.println(F("SDHC"));
      break;
    default:
      Serial.println(F("Unknown"));
  }
#endif

  // Now we will try to open the 'volume'/'partition' - it should be FAT16 or FAT32
  if (!volume.init(card)) {
    Serial.println(F("No FAT16/FAT32 partition."));
    while (1);
  }

/*
  Serial.print(F("Clusters:          "));
  Serial.println(volume.clusterCount());
  Serial.print(F("Blocks x Cluster:  "));
  Serial.println(volume.blocksPerCluster());

  Serial.print(F("Total Blocks:      "));
  Serial.println(volume.blocksPerCluster() * volume.clusterCount());
  Serial.println();

  // print the type and size of the first FAT-type volume
  uint32_t volumesize;
  Serial.print(F("Volume type is:    FAT"));
  Serial.println(volume.fatType(), DEC);

  volumesize = volume.blocksPerCluster();    // clusters are collections of blocks
  volumesize *= volume.clusterCount();       // we'll have a lot of clusters
  volumesize /= 2;                           // SD card blocks are always 512 bytes (2 blocks are 1 KB)
  Serial.print(F("Volume size (KB):  "));
  Serial.println(volumesize);
  Serial.print(F("Volume size (MB):  "));
  volumesize /= 1024;
  Serial.println(volumesize);
  Serial.print(F("Volume size (GB):  "));
  Serial.println((float)volumesize / 1024.0);

  Serial.println(F("\nFiles found on the card (name, date and size in bytes): "));
  root.openRoot(volume);

  // list all files in the card with date and size
  root.ls(LS_R | LS_DATE | LS_SIZE);
  root.close();
  */
}

void print_with_leading_zero(int n) {
  if (n < 10) {
    Serial.print('0');
  }
  Serial.print(n, DEC);
}

void print_date() {
  Serial.print(F("Date: "));
  Serial.print(now.year(), DEC);
  Serial.print('/');
  print_with_leading_zero(now.month());
  Serial.print('/');
  print_with_leading_zero(now.day());
  /*
  Serial.print(F(" ("));
  Serial.print(dayOfTheWeek[now.dayOfTheWeek()]);
  Serial.print(')');
  */
  Serial.print(' ');
  print_with_leading_zero(now.hour());
  Serial.print(':');
  print_with_leading_zero(now.minute());
  Serial.print(':');
  print_with_leading_zero(now.second());
  Serial.println();
}

void cmd_exec(char *buf) {
  switch (*buf) {
    case 'u': // uptime
        Serial.print(F("Uptime: "));
        Serial.print(now.unixtime() - boot_time, DEC);
        Serial.println();
        break;
    case 'D': // epochDate
        // Format time value: $ date "+%s 8 60 60 * * - p" | dc | pbcopy
        if (buf[1] == '\0') {
          print_date();
        }
        else if (isDigit(buf[2])) {
          rtc.adjust(DateTime(strtoul(&buf[2], NULL, 10)));
          now = rtc.now();
          print_date();
        }
        else {
          err(__LINE__, "DATE");
        }
        break;
    default:
        err(__LINE__, buf);
  }
}

static char cmdbuf[32];
static byte cmdidx = 0;

void handle_serial() {
  while (Serial.available()) {
    int ch=Serial.read();
    if ((ch == '\n') || (ch == '\r')) {
      // Parse buffer if len > 0 && len < size
      if ((cmdidx > 0) && (cmdidx < sizeof(cmdbuf))) {
        cmdbuf[cmdidx] = '\0';
        cmd_exec(cmdbuf);
      }
      cmdidx = 0;
    } else if (ch != -1) {
      // Add character to buffer if space available
      if (cmdidx < sizeof(cmdbuf)) {
        cmdbuf[cmdidx++] = (char)(ch & 0xff);
      }
    }
  }
}

void err(unsigned line, const char *buf) {
  Serial.print(F("ERR: "));
  if (buf != NULL) {
    Serial.print(line, DEC);
    Serial.print(' ');
    Serial.println(buf);
  }
  else {
    Serial.println(line, DEC);
  }
  Serial.flush();
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
    err(__LINE__, "RTC");
    while(1) delay(10);
  }
  Serial.println(__LINE__);
  
  if (! rtc.initialized() || rtc.lostPower()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  rtc.start();

  sd_init();

  now = rtc.now();
  boot_time = now.unixtime();
}

DateTime last;

void loop() {
  now = rtc.now();

  if (Serial.available()) {
    handle_serial();
  }

  digitalWrite(LED_RED, digitalRead(CARD_DET));

  if (now != last) {
    last = now;

    digitalWrite(LED_GREEN, (now.second() & 1) ? HIGH : LOW);

/*
    Serial.print(F(" since midnight 1/1/1970 = "));
    Serial.print(now.unixtime());
    Serial.print(F("s = "));
    Serial.print(now.unixtime() / 86400L);
    Serial.println('d');
*/
  }

  delay(10);
}
