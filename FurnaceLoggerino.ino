/*
 * Furnace Logger - Arduino Uno + Adafruit Logger shield
 * https://learn.adafruit.com/adafruit-data-logger-shield
 */

// I/O provided by the Adafruit Logger shield
#define SQ_WAVE_IN  (2)
#define LED_GREEN   (3)
#define LED_RED     (5)
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

#define ISO_TIME

#include <SPI.h>
#include <SD.h>
#include <RTClib.h>
#include <time.h>
#include <avr/wdt.h>

const int chipSelect = SD_CS;
RTC_PCF8523 rtc;
time_t now;
time_t boot_time;
time_t last_time;

void reboot(void) {
  wdt_disable();
  wdt_enable(WDTO_15MS);
  while (true) {
    // BUSY LOOP
  }
}

bool sd_init() {
  bool success = false;
  Serial.print(F("\nSD Initialization: "));

  if (!SD.begin(chipSelect)) {
    Serial.println(F("FAIL"));
  } else {
    Serial.println(F("Success"));
    success = true;
  }

  return success;
}

void print_date() {
  time_t time_now = time(NULL);
#ifdef ISO_TIME
  struct tm *component_time = localtime(&time_now);
  char *ascii_time = isotime(component_time);
#else
  char *ascii_time = ctime(&time_now);
#endif
  Serial.println(ascii_time);
}

void cmd_exec(char *buf) {
  bool success = true;
  switch (*buf) {
    case 'u': // uptime
        Serial.print(F("Uptime: "));
        Serial.print(difftime(now, boot_time), DEC);
        Serial.println();
        break;
    case 'D': // epochDate
        // Format time value: $ date "+%s 8 60 60 * * - p" | dc | pbcopy
        if (buf[1] == '\0') {
          print_date();
        }
        else if (isDigit(buf[2])) {
          rtc.adjust(DateTime(strtoul(&buf[2], NULL, 10)));
          now = rtc.now().secondstime();
          print_date();
          // Reboot to sync to new clock time
          reboot();
        }
        else {
          success = false;
        }
        break;
    case 'R': // reboot
        reboot();
        break;
    case 'c': // cat logfile
        {
          File data = SD.open(LOGFILE);
          if (data) {
            Serial.println(LOGFILE);
            while (data.available()) {
              Serial.write(data.read());
            }
            data.close();
          }
          else {
            Serial.println(F("READ ERROR"));
          }
        }
        break;
    default:
        success = false;
  }

  if (success == false) {
    Serial.println(F("ERROR"));
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
  
  if (! rtc.initialized() || rtc.lostPower()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  rtc.deconfigureAllTimers();
  //rtc.start();
  set_system_time(rtc.now().secondstime());

  rtc.enableSecondTimer();
  attachInterrupt(digitalPinToInterrupt(SQ_WAVE_IN), system_tick, FALLING);

  if (sd_init() == false) {
    while (true) {
      // BUSY LOOP
    }
  }

  now = time(NULL);
  boot_time = now;
  last_time = boot_time;

  // Log the boot event to the logfile
  File logfile = SD.open(LOGFILE, FILE_WRITE);
  if (logfile) {
    logfile.print((boot_time + (long)UNIX_OFFSET), DEC); // Time in seconds since y2k
    logfile.println(",BOOT");
    logfile.close();
  }
  else {
    Serial.print(F("LOGFILE ERR"));
  }
}

void loop() {
  now = time(NULL);

  if (Serial.available()) {
    handle_serial();
  }

  digitalWrite(LED_RED, digitalRead(CARD_DET));

  if (now != last_time) {
    last_time = now;

    digitalWrite(LED_GREEN, (last_time & 1) ? HIGH : LOW);
  }

  delay(10);
}
