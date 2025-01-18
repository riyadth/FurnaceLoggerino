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
#define DS18B20_PIN (7)

// Monitor inputs connected to A0/A1/A2
#define FAN_IN      (A0)
#define LOW_IN      (A1)
#define HIGH_IN     (A2)

#define LOGFILE     "FURNACE.LOG"

#define ISO_TIME

#define NUM_ITEMS(x)  (sizeof(x) / sizeof(x[0]))

#include <SPI.h>
#include <SD.h>
#include <RTClib.h>
#include <time.h>
#include <avr/wdt.h>

const int chipSelect = SD_CS;
bool card_ejected;
RTC_PCF8523 rtc;
time_t now;
time_t boot_time;
time_t last_time;
time_t last_hb_time;
time_t last_write_time;

#define HB_INTERVAL (ONE_HOUR / 4)

/*
 * Logfile functionality
 */

enum log_events {
  BOOT        = (1 << 0),
  HEARTBEAT   = (1 << 1),
  STATE_CHG   = (1 << 2),
  FAN_ON      = (1 << 3),
  LOW_ON      = (1 << 4),
  HIGH_ON     = (1 << 5),
};

typedef struct {
  uint32_t  timestamp;
  byte      event;
} log_entry_t;

log_entry_t log_buf[32];
unsigned log_idx = 0;

void log_boot(uint32_t timestamp) {
  if (log_idx < NUM_ITEMS(log_buf)) {
    log_buf[log_idx].timestamp = timestamp;
    log_buf[log_idx].event = BOOT;
    log_idx++;
  }
}

void log_heartbeat(uint32_t timestamp) {
  if (log_idx < NUM_ITEMS(log_buf)) {
    log_buf[log_idx].timestamp = timestamp;
    log_buf[log_idx].event = HEARTBEAT;
    log_idx++;
  }
}

void log_state_change(uint32_t timestamp, bool fan, bool low, bool high) {
  byte event =  STATE_CHG           |
                (fan ? FAN_ON : 0)  |
                (low ? LOW_ON : 0)  |
                (high ? HIGH_ON : 0);

  if (log_idx < NUM_ITEMS(log_buf)) {
    log_buf[log_idx].timestamp = timestamp;
    log_buf[log_idx].event = event;
    log_idx++;
  }

}

unsigned log_entries(void) {
  return log_idx;
}

void log_write(void) {
  // Log the buffered event data to the logfile
  if (card_present() && !card_write_protected()) {
    if (card_ejected) {
      // Card was ejected and re-inserted. Re-intitialize the SD subsystem.
      if (sd_init()) {
        card_ejected = false;
      }
    }
    File logfile = SD.open(LOGFILE, FILE_WRITE);
    if (logfile) {
      for (unsigned i=0; i<log_idx; i++) {
        logfile.print(log_buf[i].timestamp + (long)UNIX_OFFSET);
        if (log_buf[i].event & BOOT) {
          logfile.println(F(",BOOT"));
        }
        else if (log_buf[i].event & HEARTBEAT) {
          logfile.println(F(",HEARTBEAT"));
        }
        else if (log_buf[i].event & STATE_CHG) {
          logfile.print(',');
          logfile.print(log_buf[i].event & FAN_ON ? 1 : 0);
          logfile.print(',');
          logfile.print(log_buf[i].event & LOW_ON ? 1 : 0);
          logfile.print(',');
          logfile.println(log_buf[i].event & HIGH_ON ? 1 : 0);
        }
        logfile.close();
        log_idx = 0;
      }
    }
    else {
      Serial.println(F("LOGFILE ERROR"));
    }
  }
  else {
    Serial.println(F("CARD NOT PRESENT"));
    card_ejected = true;
  }
}

void log_print(void) {
  if (card_present()) {
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
  else {
    Serial.println(F("CARD NOT PRESENT"));
  }
}

bool card_present() {
  return (digitalRead(CARD_DET) == LOW);
}

bool card_write_protected() {
  return (digitalRead(WRITE_PROT) == HIGH);
}

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
    case 'u': // uptime in seconds
        Serial.print(F("Uptime: "));
        Serial.print(difftime(now, boot_time), DEC);
        Serial.println();
        break;
    case 'D': // epochDate - set date via time since epoch
        // Must adjust for localtime
        // Adjust time value: $ date "+%s 8 60 60 * * - p" | dc | pbcopy
        if (buf[1] == '\0') {
          // No argument, so just print the date out
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
        log_print();
        break;
    case 'f': // flush log buffer
        log_write();
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
    Serial.println("RTC ERROR");
    while (true) {
      delay(10);
    }
  }

  if (! rtc.initialized() || rtc.lostPower()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  rtc.deconfigureAllTimers();
  //rtc.start();
  set_system_time(rtc.now().secondstime());

  rtc.enableSecondTimer();
  attachInterrupt(digitalPinToInterrupt(SQ_WAVE_IN), system_tick, FALLING);

  if (card_present()) {
    if (sd_init()) {
      card_ejected = false;
    }
    else {
      // Unable to initialize card...
      while (true) {
        // BUSY LOOP
      }
    }
  }
  else {
    // Try to mount card later
    card_ejected = true;
  }

  now = time(NULL);
  boot_time = now;
  last_time = boot_time;

  // Log the boot event to the logfile
  log_boot(boot_time);
  log_write();
}

byte last_input_state = 0;

void loop() {
  now = time(NULL);

  // Process the command line
  if (Serial.available()) {
    handle_serial();
  }

  // Indicate if card is removed by lighting the red LED
  digitalWrite(LED_RED, card_present() ? LOW : HIGH);

  // Read inputs into a buffer (TODO: Debounce?)
  byte input_state = (digitalRead(FAN_IN) == LOW ? 0 : FAN_ON) |
                     (digitalRead(LOW_IN) == LOW ? 0 : LOW_ON) |
                     (digitalRead(HIGH_IN) == LOW ? 0 : HIGH_ON);

  // Check if time has changed by at least a second
  if (now != last_time) {
    last_time = now;

    digitalWrite(LED_GREEN, (last_time & 1) ? HIGH : LOW);

    if (difftime(now, last_hb_time) >= HB_INTERVAL) {
      log_heartbeat(now);
      last_hb_time = now;
    }

    // This is where we can log any changes, since a second has passed.
    if (input_state != last_input_state) {
      log_state_change(now, ((input_state & FAN_ON) != 0),
                            ((input_state & LOW_ON) != 0),
                            ((input_state & HIGH_ON) != 0));
      last_input_state = input_state;
    }

    // Write every 15 seconds or when log is more than half full
    if ((log_entries() > (NUM_ITEMS(log_buf) / 2)) || (last_write_time > 15)) {
      log_write();
      last_write_time = 0;
    }
    else {
      last_write_time++;
    }
  }

  delay(10);
}
