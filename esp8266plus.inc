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

void emit_html_begin(String *out, String htmltitle, bool onoff, int refresh)
{
  char buffer[33];

  httpServer.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  httpServer.sendHeader("Pragma", "no-cache");
  httpServer.sendHeader("Expires", "-1");
  if(refresh)httpServer.sendHeader("Refresh", itoa(refresh, buffer,10));
  httpServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
  httpServer.send(200, "text/html", ""); // Empty content inhibits Content-length header so we have to close the socket ourselves.
  httpServer.sendContent_P(templatea1);
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
  httpServer.sendContent_P(templatea2);
  httpServer.client().stop(); // Stop is needed because we sent no content length
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

void handle_custom(const char *zoom)
{
  String msg = custom_css;
  msg += "html { font-size:";
    msg += zoom;
    msg += "; }";
  httpServer.send(200, "text/css", msg);
}

void handle_login(const char *location)
{
  String msg;
  String out;

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
      Serial.printf("Login OK\n");
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
  Serial.printf("Permission denied.\n");
  delay(100);
  return;
}

bool auth_level(int auth_level)
{
  String uri = "/";

  if (httpServer.hasHeader("Cookie")){
    String cookie = httpServer.header("Cookie");
    String authk = "c=" + session_cookie;
    //Serial.printf("Cookie:'%s' Session_cookie:'%s'\n",cookie.c_str(), session_cookie.c_str());
    //Serial.printf("Privilege level:%d Requested:%d\n", privilege_level, auth_level);
    if (cookie == authk){
      if(privilege_level >= auth_level){
        return(true);
      }
      else {
        return(false);
      }
    }
  }
  Serial.printf("Wrong cookie, starting login screen\n");
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
  if(!WiFi.softAPgetStationNum() && (mymillis > (15L*60*1000)))WiFi.softAPdisconnect(true); 
}

void handle_restart()
{
  String out;

  if(!auth_level(AUTH_USER)){
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

  if(!httpServer.hasArg(name))return(false);
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
  if(!httpServer.hasArg(name))return(false);
  *ul = atol(httpServer.arg(name).c_str());
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

void try_connect_wifi(void)
{
  Serial.print("try_connect_wifi(): ");
  switch(WiFi.waitForConnectResult()) {
    case WL_CONNECTED:
    Serial.println("WIFI Connected.");
    quick_blink(10);
    break;
    case WL_NO_SSID_AVAIL:
    Serial.print("SSID Not Found: ");
    Serial.println(params.wifi_ssid);
    quick_blink(2);
    break;
    case WL_CONNECT_FAILED:
    Serial.print("Incorrect password for SSID: ");
    Serial.println(params.wifi_ssid);
    quick_blink(3);
    break;
    case WL_DISCONNECTED:
    Serial.println("Not in STATION mode!");
    quick_blink(4);
    break;
    case WL_IDLE_STATUS:
    default:
    Serial.println("Changing status...");
    quick_blink(5);
    break;
  }
}

