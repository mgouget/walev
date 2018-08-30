// esp8266plus.h MG 17/08/2018

#ifndef ESP8266PLUS_H
#define ESP8266PLUS_H

#define ON true
#define OFF false

#define AUTH_NONE 0
#define AUTH_USER 1
#define AUTH_ADMIN 2

extern int led_pin, relay_pin, button_pin, io_pin;
extern int relay_state,led_state;
extern const char PROGMEM custom_css [];
extern const char PROGMEM templatea1 [];
extern const char PROGMEM templatea2 [];
extern const char PROGMEM testhtml [];

extern ESP8266WebServer httpServer;
extern ESP8266HTTPUpdateServer httpUpdater;

extern void emit_button(String *out, String msg, const char *button_type = "primary", const char *link = NULL);
extern void emit_access_denied(const char *link = "/");
extern void emit_alert(String *out, String msg, const char *alert_type = "warning");
extern void emit_html_begin(String *out, String htmltitle, bool onoff, int refresh = 0);
extern void emit_html_end(String *out);
extern void emit_form_begin(String *out, String formtitle, String action);
extern void emit_form_col(String *out);
extern void emit_form_row(String *out);
extern void emit_form_strfield(String *out, const char *fieldname, const char *value, const char *input_type = "text");
extern void emit_form_numfield(String *out, String fieldname, long value);
extern void emit_form_end(String *out, String btn_text, bool do_cancel = true);
extern void emit_nbsp(String *out);

extern void do_led(int state);
extern void do_relay(int state);
extern void set_relay(int state);
extern bool auth_level(int auth_level);
extern void reset_login_inactivity(void);
extern void loop_login(void);
extern void handle_custom(const char *zoom);
extern void handle_logout(void);
extern void handle_restart(void);
extern void handle_login(const char *target_page);

#define SIZE_PARAM 32

struct Sminieeprom {
  int eeprom_version; // MUST be 1st
  size_t eeprom_size;  // MUST be 2nd
  char wifi_ssid[SIZE_PARAM];
  char wifi_password[SIZE_PARAM];
  char hostname[SIZE_PARAM];
  char update_path[SIZE_PARAM];
  char http_user[SIZE_PARAM];
  char http_password[SIZE_PARAM];
  char admin_user[SIZE_PARAM];
  char admin_password[SIZE_PARAM];
};

extern void save_eeprom(struct Sminieeprom *params);
extern void load_eeprom(struct Sminieeprom *params);

extern bool sanitize_strarg(const char *name, char *buf);
extern bool sanitize_ularg(const char *name, unsigned long *ul);
extern void handle_notfound();
extern void quick_blink(int num);
extern void try_connect_wifi(void);


#endif // ESP8266PLUS_H
