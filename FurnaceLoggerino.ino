/*
 * Furnace Logger - Arduino Uno + Adafruit Logger shield
 * https://learn.adafruit.com/adafruit-data-logger-shield
 */

#define VERSION "v1.0.0"

// I/O provided by the Adafruit Logger shield
#define SQ_WAVE_IN  (2)   /* Falling edge pulse at 1Hz */
#define LED_GREEN   (3)   /* HIGH to illuminate */
#define LED_RED     (5)   /* HIGH to illuminate */
#define WRITE_PROT  (8)   /* HIGH when write protected */
#define CARD_DET    (9)   /* HIGH when card absent */
#define SD_CS       (10)

// Temperature sensor using OneWire interface
#define DS18B20_PIN (7)

// Monitor inputs connected to A0/A1/A2
#define FAN_IN      (A0)  /* Open-drain, LOW when fan is on */
#define HIGH_IN     (A1)  /* Open-drain, LOW when high-heat is on */
#define LOW_IN      (A2)  /* Open-drain, LOW when low-heat is on */

#define LOGFILE     "FURNACE.LOG"

// Use ISO format for 'D'ate output on CLI
#define ISO_TIME

#define NUM_ITEMS(x)  (sizeof(x) / sizeof(x[0]))

#include <SPI.h>
#include <SD.h>
#include <RTClib.h>
#include <time.h>
#include <avr/wdt.h>

/*
 * Program global variables
 */

const int chipSelect = SD_CS;
bool card_ejected;
RTC_PCF8523 rtc;
time_t now;
time_t boot_time;
time_t last_time;
time_t last_hb_time;
time_t last_write_time;
byte last_input_state = 0xff;

#define HB_INTERVAL (ONE_HOUR / 4)
#define WRITE_INTERVAL (15 /* seconds */)

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

void log_event(uint32_t timestamp, byte event) {
  if (log_idx < NUM_ITEMS(log_buf)) {
    log_buf[log_idx].timestamp = timestamp;
    log_buf[log_idx].event = event;
    log_idx++;
  }
}

void log_boot(uint32_t timestamp) {
  log_event(timestamp, BOOT);
}

void log_heartbeat(uint32_t timestamp) {
  log_event(timestamp, HEARTBEAT);
}

void log_state_change(uint32_t timestamp, byte state) {
  log_event(timestamp, STATE_CHG | (state & (FAN_ON | LOW_ON | HIGH_ON)));
}

bool log_write_size(void) {
  // Return true if buffer is more than 3/4 full
  return (log_idx > (NUM_ITEMS(log_buf) - (NUM_ITEMS(log_buf) >> 2)));
}

bool log_data_buffered(void) {
  return (log_idx > 0);
}

bool log_write_time(void) {
  bool time_to_write = false;
  time_t now = time(NULL);

  if (log_idx > 0) {
    if (difftime(now, last_write_time) >= WRITE_INTERVAL) {
      time_to_write = true;
      // Note: last_write_time is reset by log_write() and below
    }
  }
  else {
    // Nothing to write, so postpone next write interval until data is present
    last_write_time = now;
  }

  return time_to_write;
}

void log_write(void) {
  // Log the buffered event data to the logfile
  time_t now = time(NULL);
  if (card_present() && !card_write_protected()) {
    if (card_ejected) {
      // Card was ejected and re-inserted. Re-intitialize the SD subsystem.
      if (sd_init()) {
        card_ejected = false;
      }
      else {
        // Re-mount of card failed - abort and try again next time
        Serial.println(F("CARD INIT FAILED"));
        return;
      }
    }
    if (log_idx > 0) {
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
            logfile.print(F(",STATE,"));
            logfile.print(log_buf[i].event & FAN_ON ? 1 : 0);
            logfile.print(',');
            logfile.print(log_buf[i].event & LOW_ON ? 1 : 0);
            logfile.print(',');
            logfile.println(log_buf[i].event & HIGH_ON ? 1 : 0);
          }
        }
        logfile.close();
        log_idx = 0;
        last_write_time = now;
      }
      else {
        Serial.println(F("LOGFILE ERROR"));
      }
    }
  }
  else {
    if (card_ejected == false) {
      Serial.println(F("CARD NOT PRESENT"));
      card_ejected = true;
    }
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

/*
 * Utility functions
 */

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

/*
 * SD card subsystem initialization
 */

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

/*
 * CLI functions
 */

void print_version() {
  Serial.print(F("Version: "));
  Serial.println(F(VERSION));
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
        Serial.println(F(" seconds"));
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
          rtc.start();
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
    case 'E': // TEST EVENT - force re-evaluation of inputs
        last_input_state = 0xff;
        break;
    case 'f': // flush log buffer
        log_write();
        break;
    case 'v': // Print version
        print_version();
        break;
    default:
        success = false;
  }

  if (success == true) {
    Serial.println(F("OK"));
  }
  else {
    Serial.println(F("ERROR"));
  }
}

// Static buffers for handle_serial()
static char cli_buf[32];
static byte cli_idx = 0;

void handle_serial() {
  while (Serial.available()) {
    int ch=Serial.read();
    if ((ch == '\n') || (ch == '\r')) {
      // Parse buffer if len > 0 && len < size
      if ((cli_idx > 0) && (cli_idx < sizeof(cli_buf))) {
        cli_buf[cli_idx] = '\0';
        cmd_exec(cli_buf);
      }
      cli_idx = 0;
    } else if (ch != -1) {
      // Add character to buffer if space available
      if (cli_idx < sizeof(cli_buf)) {
        cli_buf[cli_idx++] = (char)(ch & 0xff);
      }
    }
  }
}

/*
 * Arduino setup() and loop() functions
 */

void setup() {
  Serial.begin(115200);

  print_version();

  pinMode(LED_GREEN, OUTPUT);
  digitalWrite(LED_GREEN, LOW);
  pinMode(LED_RED, OUTPUT);
  digitalWrite(LED_RED, LOW);

  pinMode(SQ_WAVE_IN, INPUT_PULLUP);
  pinMode(CARD_DET, INPUT_PULLUP);
  pinMode(WRITE_PROT, INPUT_PULLUP);

  pinMode(FAN_IN, INPUT_PULLUP);
  pinMode(LOW_IN, INPUT_PULLUP);
  pinMode(HIGH_IN, INPUT_PULLUP);

  if (! rtc.begin()) {
    Serial.println("RTC ERROR");
    while (true) {
      delay(10);
    }
  }

  if (! rtc.initialized() || rtc.lostPower()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    rtc.start();
  }

  rtc.deconfigureAllTimers();
  set_system_time(rtc.now().secondstime());

  // Enable RTC chip 1Hz output and enable Arduino interrupt for timekeeping
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

  // Initialize time variables
  now = time(NULL);
  boot_time = now;
  last_time = boot_time;
  last_hb_time = boot_time;

  // Log the boot event to the logfile
  log_boot(boot_time);
  log_write();

  Serial.println(F("BOOT"));
}

void loop() {
  now = time(NULL);

  // Process the command line
  if (Serial.available()) {
    handle_serial();
  }

  // Indicate if card is removed by lighting the red LED
  // digitalWrite(LED_RED, card_present() ? LOW : HIGH);

  // Read inputs into a buffer (TODO: Debounce?, Polarity?)
  byte input_state = (digitalRead(FAN_IN) == HIGH ? 0 : FAN_ON) |
                     (digitalRead(LOW_IN) == HIGH ? 0 : LOW_ON) |
                     (digitalRead(HIGH_IN) == HIGH ? 0 : HIGH_ON);

  // Check if time has changed by at least a second
  if (now != last_time) {
    last_time = now;

    // Toggle the green LED each time through this loop (0.5Hz)
    digitalWrite(LED_GREEN, (last_time & 1) ? HIGH : LOW);

    // Toggle the red LED if there is a card present and data to write
    if (card_present()) {
      // Opposite pattern to green LED
      if (log_data_buffered()) {
        digitalWrite(LED_RED, (last_time & 1) ? LOW : HIGH);
      }
      else {
        // Turn off the LED
        digitalWrite(LED_RED, LOW);
      }
    }
    else {
        digitalWrite(LED_RED, HIGH);
    }

    // Log a heartbeat event if heartbeat interval has passed
    if (difftime(now, last_hb_time) >= HB_INTERVAL) {
      log_heartbeat(now);
      last_hb_time = now;
    }

    // This is where we can log any changes, since a second has passed.
    if (input_state != last_input_state) {
      log_state_change(now, input_state);
      last_input_state = input_state;
    }

    // Write out log periodically or when it has a bunch of entries in it
    if (log_write_time() || (log_write_size())) {
      log_write();
    }
  }

  delay(10);
}
