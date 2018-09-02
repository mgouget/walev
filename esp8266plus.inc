// esp8266plus.inc MG 17/08/18

#include "esp8266plus.h"

ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

void do_led(int state) { analogWrite(led_pin, state ? 0 : 1000); led_state = state; }
void do_relay(int state) { digitalWrite(relay_pin, state); relay_state = state; }
void set_relay(int state) { do_led(state); do_relay(state); }

//const char* ssid     = "";
//const char* password = "";

bool login_lock = false; //This bool is used to control device lockout

String session_cookie; //this is cookie buffer
int privilege_level = AUTH_NONE;

String anchars = "abcdefghijklmnopqrstuvwxyz0123456789";

unsigned long logincld = millis(), reqmillis = millis(), tempign = millis(); //First 2 timers are for lockout and last one is inactivity timer

uint8_t i, trycount = 0; // i is used for for index, trycount will be our buffer for remembering how many false entries there were

void gencookie(){
  session_cookie = "";
  for( i = 0; i < 32; i++) session_cookie += anchars[random(0, anchars.length())]; //Using randomchar from anchars string generate 32-bit cookie
}

void emit_button(String *out, String msg, const char *button_type, const char *link)
{
  if(link && *link){
    *out += "<a href=\"";
    *out += link;
    *out += "\">";
  }
  *out += "<button type=\"button\" class=\"btn btn-";
  *out += button_type;
  *out += " m-1 btn-lg \">";
  *out += msg;
  *out += "</button>";
  if(link && *link) *out += "</a>";
}

void emit_alert(String *out, String msg, const char *alert_type)
{
  *out += "<div class=\"alert alert-";
  *out += alert_type;
  *out += "\">\n";
  *out += msg;
  *out += "</div>\n";
}

extern const char PROGMEM ___bootstrap_min_css_gz [];
extern unsigned int ___bootstrap_min_css_gz_len;

void emit_not_modified(void)
{
  String header = "HTTP/1.1 304 Not Modified\r\nCache-Control: public, max-age=30672000\r\n\r\n";
  httpServer.sendContent(header);
}

void handle_bootstrap_min_css()
{
  /*
  if(security_level() > AUTH_NONE){
    DEBUG("Sending 304 Not modified bootstrap_min_css_gz\n");
    emit_not_modified();
    return;
  }
  DEBUG("Sending bootstrap_min_css_gz\n");
  */
  httpServer.sendHeader("Cache-Control", "public, max-age=max-age=30672000");
  httpServer.sendHeader("Content-Encoding", "gzip");
  httpServer.send_P(200, "text/css", ___bootstrap_min_css_gz, ___bootstrap_min_css_gz_len);
  //DEBUG("Sending done.\n");
}

static const char *zoom;

void set_zoom(const char *z)
{
  zoom = z;
}

  
void emit_html_begin(String *out, String htmltitle, bool onoff, int refresh)
{
  char buffer[33];

  httpServer.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  httpServer.sendHeader("Pragma", "no-cache");
  httpServer.sendHeader("Expires", "-1");
  if(refresh)httpServer.sendHeader("Refresh", itoa(refresh, buffer,10));
  httpServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
  httpServer.send(200, "text/html", ""); // Empty content inhibits Content-length header so we have to close the socket ourselves.
  httpServer.sendContent_P(templatea1a);
  if(zoom && *zoom){
    *out = "html { font-size:";
    *out += zoom;
    *out += "; }";
    httpServer.sendContent(*out);
  }
  httpServer.sendContent_P(templatea1b);
  *out = "<title>";
  *out += htmltitle;
  *out += "</title></head><body><div class=\"container\">\n";
  *out = "<h3 class=\"";
  *out += onoff ? "bg-primary" : "bg-info";
  *out += "\"><center>";
  *out += htmltitle;
  *out += "</center></h3>";
}

void emit_html_end(String *out)
{
  httpServer.sendContent(*out);
  //DEBUG("emit_html_end(): emitting '%s'\n", (*out).c_str());
  *out = "";
  httpServer.sendContent_P(templatea2);
  //DEBUG("emit_html_end(): normal end\n");
}

void emit_form_begin(String *out, String formtitle, String action)
{
  *out += "<form class=\"text border p-1\" action=\"";
  *out += action;
  *out += "\" method=\"post\">\n<div class=\"form-group\">\n";
  *out += "<p><center><h4 class=\"mgtitle\">";
  *out += formtitle;
  *out += "</h4></center></p>";
  *out += "<div class=\"row\">\n<div class=\"col\">\n";
}

void emit_form_col(String *out) { *out += "\n</div>\n<div class=\"col\">\n"; }
void emit_form_row(String *out) { *out += "\n</div></div>\n<div class=\"row top-buffer\">\n<div class=\"col\">\n"; }

void emit_form_strfield(String *out, const char *fieldname, const char *value, const char *input_type)
{
  *out += "<label class=\"mglabel\" >";
  *out += fieldname;
  *out += ":</label>";
  *out += "<input class=\"mginput\" type=\"";
  *out += input_type;
  *out += "\" class=\"mgbutton\" name=\"";
  *out += fieldname;
  *out += "\"";
  if(strcmp(input_type, "password")){
    *out += "value=\"";
    *out += value;
    *out += "\"";
  }
  *out += ">";
}

void emit_form_numfield(String *out, String fieldname, long value)
{
  char buf[33];
  itoa (value, buf, 10);
  emit_form_strfield(out, fieldname.c_str(), buf);
}

void emit_form_end(String *out, String btn_text, bool do_cancel)
{
  *out += "</div></div></div>";
  *out += "<div class=\"container\"><center><div class=\"span1\">\n";
  if(do_cancel) emit_button(out, "Cancel", "warning", "/");
  *out += "<button type=\"submit\" class=\"btn btn-primary m-1 btn-lg \">";
  *out += btn_text;
  *out += "</button>";
  *out += "</div></center></div\n";
  *out += "</form>";
}

void emit_nbsp(String *out)
{
  *out += "&nbsp;";
}

void handle_login(const char *location)
{
  String msg;
  String out;

  //DEBUG("Login Entering\n");
  if(!session_cookie.length())gencookie();
  if (httpServer.hasArg("User") && httpServer.hasArg("Password")){
    bool valid_auth = false;
    if (httpServer.arg("User") == params.http_user &&  httpServer.arg("Password") == params.http_password && !login_lock){
      valid_auth = true;
      privilege_level = AUTH_USER;
    }
    else if (httpServer.arg("User") == params.admin_user &&  httpServer.arg("Password") == params.admin_password && !login_lock){
      valid_auth = true;
      privilege_level = AUTH_ADMIN;
    }
    if(valid_auth){
      //if above values are good, send 'Cookie' header with variable c, with format 'c=session_cookie'
      String header = "HTTP/1.1 301 OK\r\nSet-Cookie: c=" + session_cookie + "\r\nLocation: ";
      header += location;
      header += " \r\nCache-Control: no-cache\r\n\r\n";
      httpServer.sendContent(header);
      trycount = 0; //With good headers in mind, reset the trycount buffer
      DEBUG("Login OK\n");
      delay(100);
      return;
    }
    msg = "<center><br>";
    if (trycount != 10 && !login_lock)trycount++; //If system is not locked up the trycount buffer
    if (trycount < 10 && !login_lock){ //We go here if systems isn't locked out, we give user 10 times to make a mistake after we lock down the system, thus making brute force attack almost imposible
      msg += "Wrong username/password<br>";
      msg += (10 - trycount);
      msg += " tries left.";
      logincld = millis(); //Reset the logincld timer, since we still have available tries
    }
    if (trycount == 10){ //If too much bad tries
      if(login_lock){
        msg += "Too many invalid login attempts. Device locked for ";
        msg += 5 - ((millis() - logincld) / 60000); //Display lock time remaining in minutes
        msg += " minutes.";
      }
      else{
        logincld = millis();
        login_lock = true;
        msg += "Too many invalid login attempts. Please wait 5 minutes before login again."; //This happens when your device first locks down
      }
    }
  }
  emit_html_begin(&out, params.hostname, relay_state);
  if(msg.length()){
    emit_alert(&out, msg, "danger");
  }
  emit_form_begin(&out, "LOGIN", "/login");
  emit_form_strfield(&out, "User", "");
  emit_form_col(&out);
  emit_nbsp(&out);
  emit_form_row(&out);
  emit_form_strfield(&out, "Password", "", "password");
  emit_form_col(&out);
  emit_nbsp(&out);
  emit_form_end(&out, "OK", false);
  emit_html_end(&out);

  return;
}

void emit_access_denied(const char *link)
{
  String out;
  emit_html_begin(&out, params.hostname, relay_state);
  emit_alert(&out, "<center>Access Denied</center>", "danger");
  out += "<div class=\"container\"><center><div class=\"span1\">\n";
  emit_button(&out, "OK", "primary", link);
  out += "</div></center></div\n";
  emit_html_end(&out);
  DEBUG("Permission denied.\n");
  delay(100);
  return;
}

int security_level(void)
{
  if (httpServer.hasHeader("Cookie")){
    String cookie = httpServer.header("Cookie");
    String authk = "c=" + session_cookie;
    //DEBUG("Cookie:'%s' Session_cookie:'%s'\n",cookie.c_str(), session_cookie.c_str());
    if (cookie == authk){
      //DEBUG("Privilege level:%d\n", privilege_level);
      return(privilege_level);
    }
    else {
      return(AUTH_NONE);
    }
  }
}

bool is_authenticated(int auth_level)
{
  if(security_level() > AUTH_NONE){
    if(privilege_level >= auth_level){
      return(true);
    }
    else {
      emit_access_denied("/");
      return(false);
    }
  }
  DEBUG("Wrong cookie, starting login screen\n");
  String uri = "/";
  if (httpServer.hasHeader("URI")){
    uri = httpServer.header("uri");
  }
  if(uri == "/login") uri = "/";
  handle_login(uri.c_str());
  return(false);
}

void reset_login_inactivity(void)
{
  tempign = millis();
}

void loop_login(void)
{
  time_t mymillis = millis();
  if(login_lock && abs(millis() - logincld) > 300000){
    login_lock = false;
    trycount = 0;
    logincld = mymillis; //After 5 minutes is passed unlock the system
  }
  if(!login_lock && abs(mymillis - logincld) > 60000){
    trycount = 0;
    logincld = mymillis;
    //After minute is passed without bad entries, reset trycount
  }
  if(abs(mymillis - tempign) > 120000){
    gencookie();
    reset_login_inactivity();
    //if there is no activity from loged on user, change the generate a new cookie. This is more secure than adding expiry to the cookie header
  }
  // No AP 15 minutes after booting if none connected
  if(!WiFi.softAPgetStationNum() && (mymillis > (15L*60*1000)))WiFi.softAPdisconnect(true); 
}

void handle_restart()
{
  String out;

  if(!is_authenticated(AUTH_USER)){
    emit_access_denied("/login");
    return;
  }
  emit_html_begin(&out, params.hostname, relay_state);
  emit_alert(&out, "Restarting device...", "warning");
  out += "<div class=\"container\"><center><div class=\"span1\">\n";
  emit_button(&out, "OK", "primary", "/");
  out += "</div></center></div\n";
  emit_html_end(&out);
  set_relay(OFF);
  delay(1000);
  ESP.restart();
}

void handle_logout(){
  String header = "HTTP/1.1 301 OK\r\nSet-Cookie: c=0\r\nLocation: /login\r\nCache-Control: no-cache\r\n\r\n"; //Set 'c=0', it users header, effectively deleting it's header
  httpServer.sendContent(header);
}

bool sanitize_strarg(const char *name, char *buf)
{
  const char *pcd;
  char *pca;
  int i;
  String s;

  if(!httpServer.hasArg(name)){
    DEBUG("sanitize_ularg() name='%s' NOT FOUND!\n", name);
    return(false);
  }
  s = httpServer.arg(name);
  for(i = 1, pcd = s.c_str(), pca = buf; i < SIZE_PARAM; i++){
    if(!*pcd) break;
    *pca++ = *pcd++;
  }
  *pca = 0;
  return(true);
}

bool sanitize_ularg(const char *name, unsigned long *ul)
{
  if(!httpServer.hasArg(name)){
    DEBUG("sanitize_ularg() name='%s' NOT FOUND!\n", name);
    return(false);
  }
  *ul = atol(httpServer.arg(name).c_str());
  //DEBUG("sanitize_ularg('%s','%s') = %ul\n", name, httpServer.arg(name).c_str(), *ul);
  return(true);
}

void handle_notfound()
{
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += httpServer.uri();
  message += "\nMethod: ";
  message += (httpServer.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += httpServer.args();
  message += "\n";
  for (byte i = 0; i < httpServer.args(); i++) {
    message += " " + httpServer.argName(i) + ": " + httpServer.arg(i) + "\n";
  }
  message += "<p><a href=\"/\"><button>OK</button></a></p>";
  httpServer.send(404, "text/html", message);
  tempign = millis();
}

void quick_blink(int num)
{
  int i, j;
  num *= 2;
  do_led(OFF);
  for(i = 0; i < num;i++){
    j = 1 - led_state;
    do_led(j);
    delay(50);
  }
}

/*
void try_connect_wifi(const char *ssid, const char *password)
{
  static int last_status = WL_IDLE_STATUS;
  int status = WiFi.status();
  DEBUG("try_connect_wifi(): ");
  switch(WiFi.status()) {
    case WL_CONNECTED:
    if(last_status != status){
      ArduinoOTA.begin();
      httpServer.begin();
      MDNS.begin(params.hostname);
      MDNS.addService("http", "tcp", 80);
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
    WiFi.begin(ssid, password);
  }
  last_status = status;
}
*/

