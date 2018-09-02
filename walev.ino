// walev.ino MG 17/08/2018
// Maintains water level

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ESP8266HTTPUpdateServer.h>
#include <EEPROM.h>
#include "esp8266plus.h"
#include <pgmspace.h>

#include "arrays.cpp"
int led_pin = 13;
int relay_pin = 12;
int button_pin = 0;
int io_pin = 14;

int led_state, relay_state;

// Soft AP network parameters
IPAddress apIP(192, 168, 4, 1);
IPAddress netMsk(255, 255, 255, 0);

#define ST_STARTING 0
#define ST_PAUSING 1
#define ST_SENSING 2
#define ST_DRIVING 3
#define ST_FORCING 4
#define ST_ALARM 5 // Must be last

int state = ST_STARTING;
time_t next_time;
time_t max_on_time = 0;
time_t mytime;
int nb_sense;
int nb_dry;
int relay_status;


#define FIRMWARE_VERSION 7
#define EEPROM_VERSION 2

// If order of parameters changed, must increase version.
// If parameters added at end, can keep version.
struct Sparameters {
  int eeprom_version; // MUST be 1st
  size_t eeprom_size;  // MUST be 2nd
  char wifi_ssid[SIZE_PARAM];
  char wifi_password[SIZE_PARAM];
  char hostname[SIZE_PARAM];
  char zoom[SIZE_PARAM];
  char update_path[SIZE_PARAM];
  char http_user[SIZE_PARAM];
  char http_password[SIZE_PARAM];
  char admin_user[SIZE_PARAM];
  char admin_password[SIZE_PARAM];
  // DO NOT MODIFY ABOVE or change EEPROM Version
  int time_pause; // seconds
  int time_sense; // seconds
  int time_drive; // seconds
  int max_forced_duration; // minutes
  int max_on_duration; // minutes
  int min_dry_percent;
};

struct Sparameters params;

struct Sparameters factory_params = {
  EEPROM_VERSION,
  sizeof(struct Sparameters),
  "TATANKA5G", /* wifi_ssid */
  "tatanka5", /* wifi_password */
  "PADUA1", /* hostname */
  "3vw", /* zoom */
  "/firmware", /* update_path */
  "user", /* http_user */
  "password", /* http_password */
  "admin", /* admin_user */
  "password", /* admin_password */
  2, // time_pause (s)
  2, // time_sense (s)
  10, // time_drive (s)
  60, // max_forced_duration (min)
  2*60, // max_on_duration (min)
  50, // min_dry_percent
};

void save_eeprom()
{
  Serial.print("Flashing ");
  Serial.print(params.eeprom_size);
  Serial.println(" bytes to EEPROM...");
  EEPROM.begin(512);
  EEPROM.put(0, params);
  EEPROM.commit();
  EEPROM.end();
  Serial.println("EEPROM flashed.");
}

void load_eeprom()
{
  int i;
  int sz;
  byte *pbd, *pba;

  EEPROM.begin(512);
  EEPROM.get(0, params);
  EEPROM.end();
  DEBUG("EEPROM_VERSION:%d ; Read:%d\n", factory_params.eeprom_version, params.eeprom_version);
  if(params.eeprom_version != factory_params.eeprom_version){
    DEBUG("WARNING, Factory reset!\n");
    memcpy(&params, &factory_params, sizeof(params));
    save_eeprom();
  }
  else {
    sz = params.eeprom_size;
    if(params.eeprom_size != sz){
      pbd = (byte *)&factory_params;
      pbd += sz;
      pba = (byte *)&params;
      pba += sz;
      memcpy(pba, pbd, factory_params.eeprom_size - sz);
      params.eeprom_size = factory_params.eeprom_size;
      save_eeprom();
      DEBUG("EEPROM updated.\n");
    }
    else {
      DEBUG("Parameters imported.\n");
    }
  }
}

void handle_root()
{
  String out;
  String message;
  const char *alert_type = "default";

  //DEBUG("handle_root(): Entering\n");
  if(!is_authenticated(AUTH_USER)){
    DEBUG("handle_root(): Not authenticated\n");
    return;
  }
  emit_html_begin(&out, params.hostname, relay_state, 5);
  //emit_html_begin(&out, params.hostname, relay_state);
  message = "<center><h4>Switch is ";
  if(state == ST_FORCING){
    message += " forced to ";
    if(relay_state == ON){
      message += "ON";
      alert_type = "warning";
    }
    else {
      message += "OFF";
      alert_type = "success";
    }
  }
  else {
    if(relay_status == ON){
      message += "ON";
      alert_type = "warning";
    }
    else {
      message += "OFF";
      alert_type = "success";
    }
  }
  message += "</h4></center>";
  emit_alert(&out, message, alert_type);
  out += "<div class=\"container\">\n";
  out += "<center><div class=\"span2\">\n";
  if(state == ST_ALARM){
    emit_alert(&out, "ALARM", "danger");
  }
  else {
    if(state == ST_FORCING){
      emit_button(&out, "Back Normal", "primary", "/back_normal");
    }
    emit_button(&out, "OFF", "success", "/off");
    emit_button(&out, "ON", "danger", "/on");
  }

  if(security_level() >= AUTH_ADMIN){
    out += "</div><div class=\"span2\">\n";
    emit_button(&out, "Parameters", "info", "/parameters");
    emit_button(&out, "Firmware", "info", "/firmware");
  }
  out += "</div><div class=\"span2\">\n";
  emit_button(&out, "Restart", "info", "/restart");
  emit_button(&out, "Logout", "info", "/logout");
  out += "</div></center>\n";
  out += "</div>\n";

  emit_html_end(&out);
  reset_login_inactivity();
  //DEBUG("handle_root(): Normal end\n");
}

void handle_back_normal()
{
  if(!is_authenticated(AUTH_USER))return;
  set_relay(OFF);
  relay_status = OFF;
  state = ST_STARTING;
  next_time = millis();
  httpServer.sendContent("HTTP/1.1 301 OK\nLocation: /\nCache-Control: no-cache\n\n");
  reset_login_inactivity();
}

void handle_onoff(int onoff)
{
  if(!is_authenticated(AUTH_USER))return;
  // If forced on, reset to normal after max_forced_duration minutes
  // If forced off, continue forever
  state = ST_FORCING;
  set_relay(onoff);
  relay_status = onoff;
  if(relay_state == ON){
    next_time = millis() + (params.max_forced_duration * 60 * 1000);
    DEBUG("Setting ST_FORCING, millis=%lu next_time=%lu\n", millis(), next_time);
  }
  else next_time = 0;
  httpServer.sendContent("HTTP/1.1 301 OK\nLocation: /\nCache-Control: no-cache\n");
  reset_login_inactivity();
}

#define ARG_TIME_PAUSE "time_pause (s)"
#define ARG_TIME_SENSE "time_sense (s)"
#define ARG_TIME_DRIVE "time_drive (s)"
#define ARG_MAX_FORCED_DURATION "max_forced_dur.(min)"
#define ARG_MAX_ON_DURATION "max_on_duration (min)"
#define ARG_MIN_DRY_PERCENT  "min_dry_percent (%)"

void handle_parameters()
{
  String out;

  if(!is_authenticated(AUTH_ADMIN))return;
  emit_html_begin(&out, params.hostname, relay_state);

  {
    char msg[80];
    sprintf(msg, "<center>Device #%08X Firmware V%d</center>", ESP.getChipId(), FIRMWARE_VERSION);
    emit_alert(&out, msg, "info");
  }

  emit_form_begin(&out, "PARAMETERS", "/update_parameters");
  emit_form_strfield(&out, "hostname", params.hostname);
  emit_form_col(&out);
  emit_form_strfield(&out, "zoom", params.zoom);
  emit_form_row(&out);
  emit_form_strfield(&out, "wifi_ssid", params.wifi_ssid);
  emit_form_col(&out);
  emit_form_strfield(&out, "wifi_password", params.wifi_password);
  emit_form_row(&out);
  emit_form_strfield(&out, "http_user", params.http_user);
  emit_form_col(&out);
  emit_form_strfield(&out, "http_password", params.http_password);
  emit_form_row(&out);
  emit_form_strfield(&out, "admin_user", params.admin_user);
  emit_form_col(&out);
  emit_form_strfield(&out, "admin_password", params.admin_password);
  emit_form_row(&out);
  emit_form_numfield(&out, ARG_TIME_PAUSE, params.time_pause);
  emit_form_col(&out);
  emit_form_numfield(&out, ARG_TIME_SENSE, params.time_sense);
  emit_form_row(&out);
  emit_form_numfield(&out, ARG_TIME_DRIVE, params.time_drive);
  emit_form_col(&out);
  emit_form_numfield(&out, ARG_MAX_FORCED_DURATION, params.max_forced_duration);
  emit_form_row(&out);
  emit_form_numfield(&out, ARG_MAX_ON_DURATION, params.max_on_duration);
  emit_form_col(&out);
  emit_form_numfield(&out, ARG_MIN_DRY_PERCENT, params.min_dry_percent);
  emit_form_end(&out, "Update Parameters");
  emit_html_end(&out);
  reset_login_inactivity();
}

void handle_update_parameters()
{
  bool must_reboot = false;
  char str[SIZE_PARAM];
  unsigned long ul;
  if(!is_authenticated(AUTH_ADMIN))return;
  if(sanitize_strarg("wifi_ssid", str)){
    if(strcmp(str, params.wifi_ssid)){
      must_reboot = true;
      strcpy(params.wifi_ssid, str);
    }
  }
  if(sanitize_strarg("wifi_password", str)){
    if(strcmp(str, params.wifi_password)){
      strcpy(params.wifi_password, str);
      must_reboot = true;
    }
  }
  if(sanitize_strarg("hostname", str))strcpy(params.hostname, str);
  if(sanitize_strarg("zoom", str))strcpy(params.zoom, str);
  if(sanitize_strarg("update_path", str))strcpy(params.update_path, str);
  if(sanitize_strarg("http_user", str)){
    if(strcmp(str, params.http_user)){
      must_reboot = true;
      strcpy(params.http_user, str);
    }
  }
  if(sanitize_strarg("http_password", str)){
    if(strcmp(str, params.http_password)){
      must_reboot = true;
      strcpy(params.http_password, str);
    }
  }
  if(sanitize_strarg("admin_user", str)){
    if(strcmp(str, params.admin_user)){
      must_reboot = true;
      strcpy(params.admin_user, str);
    }
  }
  if(sanitize_strarg("admin_password", str)){
    if(strcmp(str, params.admin_password)){
      must_reboot = true;
      strcpy(params.admin_password, str);
    }
  }
  if(sanitize_ularg(ARG_TIME_PAUSE, &ul))params.time_pause = ul;
  if(sanitize_ularg(ARG_TIME_SENSE, &ul))params.time_sense = ul;
  if(sanitize_ularg(ARG_TIME_DRIVE, &ul))params.time_drive = ul;
  if(sanitize_ularg(ARG_MAX_FORCED_DURATION, &ul))params.max_forced_duration = ul;
  if(sanitize_ularg(ARG_MAX_ON_DURATION, &ul))params.max_on_duration = ul;
  if(sanitize_ularg(ARG_MIN_DRY_PERCENT, &ul))params.min_dry_percent = ul;
  save_eeprom();
  String out;
  emit_html_begin(&out, params.hostname, relay_state);
  String message = "<center>Parameters updated.";
  if(must_reboot){
    message += "<br>You must restart device for changes to take effect.";
  }
  message += "</center>";
  emit_alert(&out, message, "warning");
  out += "<div class=\"container\"><center><div class=\"span1\">\n";
  emit_button(&out, "OK", "primary", "/");
  out += "</div></center></div\n";
  emit_html_end(&out);
  reset_login_inactivity();
}

void try_connect_wifi(void)
{
  static int last_status = WL_IDLE_STATUS;
  int status = WiFi.status();
  DEBUG("try_connect_wifi(): ");
  switch(WiFi.status()) {
    case WL_CONNECTED:
    if(last_status != status){
      DEBUG("WIFI Connected.\n");
      quick_blink(10);
    }
    break;
    case WL_NO_SSID_AVAIL:
    DEBUG("SSID Not Found: \n", params.wifi_ssid);
    quick_blink(2);
    break;
    case WL_CONNECT_FAILED:
    DEBUG("Incorrect password for SSID:'%s' \n", params.wifi_ssid);
    quick_blink(3);
    break;
    case WL_DISCONNECTED:
    DEBUG("Disconnected...\n");
    break;
    case WL_IDLE_STATUS:
    default:
    DEBUG("Changing status...\n");
    quick_blink(5);
    break;
  }
  if(status != WL_CONNECTED){
    DEBUG("WFI AP: Trying to reconnect...\n");
    WiFi.begin(params.wifi_ssid, params.wifi_password);
  }
  last_status = status;
}


void setup() {
  Serial.begin(115200);
  Serial.println("Booting");
  { // Avoid wasting memory!
    char str[32];
    sprintf(str, "ESP_%X", ESP.getChipId());
    strcpy(factory_params.hostname, str);
  }
  memcpy(&params, &factory_params, sizeof(params));
  set_zoom(params.zoom);

  pinMode(button_pin, INPUT_PULLUP);
  pinMode(io_pin, INPUT_PULLUP);

  pinMode(relay_pin, OUTPUT);
  pinMode(led_pin, OUTPUT);
  set_relay(OFF);
  /* configure dimmers, and OTA server events */
  analogWriteRange(1000);
  analogWrite(led_pin, 990);

  load_eeprom();

  // WIFI
  WiFi.hostname(params.hostname);
  WiFi.setAutoReconnect(false); // Otherwise: infinite loop when can't connect...
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(params.wifi_ssid, params.wifi_password);
  WiFi.softAPConfig(apIP, apIP, netMsk);
  WiFi.softAP(params.hostname);
  //delay(500); // Without delay I've seen the IP address blank
  ArduinoOTA.begin();
  httpServer.begin();
  MDNS.begin(params.hostname);
  MDNS.addService("http", "tcp", 80);

  // OTA
  ArduinoOTA.setHostname(params.hostname);
  ArduinoOTA.setPassword(params.admin_password);
  ArduinoOTA.onStart([]() { // switch off all the PWMs during upgrade
    do_led(OFF);
  });
  ArduinoOTA.onEnd([]() { // do a fancy thing with our board led at end
    for (int i = 0; i < 30; i++) {
      analogWrite(led_pin, (i * 100) % 1001);
      delay(50);
    }
  });
  ArduinoOTA.onError([](ota_error_t error) {
    (void)error;
    ESP.restart();
  });
  ArduinoOTA.begin();

  // httpserver
  httpServer.onNotFound(handle_notfound);
  httpServer.on("/", handle_root);
  httpServer.on("/on", [](){handle_onoff(ON);});
  httpServer.on("/off", [](){handle_onoff(OFF);});
  httpServer.on("/back_normal", handle_back_normal);
  httpServer.on("/restart", handle_restart);
  httpServer.on("/parameters", handle_parameters);
  httpServer.on("/update_parameters", handle_update_parameters);
  httpServer.on("/login", [](){handle_login("/");});
  httpServer.on("/logout", handle_logout);
  httpServer.on("/bootstrap_min.css", handle_bootstrap_min_css);
  const char * headerkeys[] = {"User-Agent","Cookie"} ;
  size_t headerkeyssize = sizeof(headerkeys)/sizeof(char*);
  httpServer.collectHeaders(headerkeys, headerkeyssize ); //These 3 lines tell esp to collect User-Agent and Cookie in http header when request is made
  httpServer.begin();

  httpUpdater.setup(&httpServer, params.update_path, params.admin_user, params.admin_password);

  DEBUG("Ready\n");
  state = ST_STARTING;
  next_time = millis();
}

void loop() {
  int i;
  time_t time1;

  ArduinoOTA.handle();
  httpServer.handleClient();
  if(state == ST_ALARM){
    quick_blink(1);
    return;
  }
  if(!digitalRead(button_pin)){
    mytime = millis();
    while(yield(),!digitalRead(button_pin));
    mytime = millis() - mytime;
    if(mytime > (10*1000)){
      DEBUG("WARNING: Factory reset!\n");
      memcpy(&params, &factory_params, sizeof(params));
      save_eeprom();
      quick_blink(30);
      ESP.restart();
    }
    else if(mytime > (2*1000)){
      DEBUG("Back to normal\n");
      set_relay(OFF);
      state = ST_STARTING;
      next_time = millis();
      quick_blink(3);
    }
    else {
      // If forced on, reset to normal after max_forced_duration minutes
      // If forced off, continue forever
      state = ST_FORCING;
      set_relay(1-relay_state);
      if(relay_state == ON){
	next_time = millis() + (params.max_forced_duration * 60 * 1000);
	DEBUG("Setting ST_FORCING, millis=%lu next_time=%lu\n", millis(), next_time);
      }
      else next_time = 0;
    }
  }
  mytime = millis();
  if(!mytime % (4 * 1024)){
    loop_login(); // Update logout every ~5 seconds
    if(WiFi.status() != WL_CONNECTED)try_connect_wifi();
  }
  if((next_time != 0) && (mytime > next_time)){
    DEBUG("millis=%lu next_time=%lu\n"); mytime, next_time;
    state++;
    if(state > ST_DRIVING)state = ST_PAUSING;
    switch(state){
    case ST_PAUSING:
      if(params.time_pause) set_relay(OFF); // Dont stop relay if no time_pause
      // Manage millis() overflow after 49 days
      // We force a reset every 20 days ;-lso avoid time becoming negative ;-)
      if(mytime > (1000L * 60 * 60 * 24 * 20)){
        DEBUG("Resetting in order to avoid time overflow!\n");
        ESP.restart();
      }
      mytime = millis();
      next_time = mytime + 1000 * params.time_pause;
      DEBUG("PAUSING\n");
      break;
    case ST_SENSING:
      nb_dry = 0;
      nb_sense = 0;
      next_time = mytime + 1000 * params.time_sense;
      DEBUG("SENSING\n");
      break;
    case ST_DRIVING:
      DEBUG("nb_dry=%d nb_sense=%d\n", nb_dry, nb_sense);
      i = (nb_dry * 100) / nb_sense;
      DEBUG("DRIVING\n");
      if(i > params.min_dry_percent){
        if(!max_on_time){
	  max_on_time = mytime + params.max_on_duration * (1000 * 60); 
	  DEBUG("Start forced ON time, millis=%ul, max_ontime:%ul\n", millis(), max_on_time);
	}
        else if(mytime > max_on_time){
          DEBUG("ALARM\n");
          set_relay(OFF);
	  relay_status = OFF;
          state = ST_ALARM;
          break;
        }
        DEBUG("ON\n");
        set_relay(ON);
	relay_status = ON;
      }
      else {
        DEBUG("OFF\n");
        set_relay(OFF);
	relay_status = OFF;
        max_on_time = 0;
      }
      next_time = mytime + 1000 * params.time_drive;
      break;
    }
  }
  if(state == ST_SENSING){
    i = digitalRead(io_pin);
    if(!i)nb_dry++;
    nb_sense++;
    delay(1);
  }
  if((state == ST_SENSING) && !(mytime % 16)){
    i = 1 - led_state;
    do_led(i);
  }
}

#include "/home/mgouget/esp8266/walev/esp8266plus.inc"
