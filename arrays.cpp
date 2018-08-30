#include <pgmspace.h>

const char custom_css[] PROGMEM = 
".top-buffer { margin-top:2vh; }\n"
".mgtitle {\n"
"  font-weight: bold;\n"
"}\n"
".mglabel {\n"
"  font-weight: bold;\n"
"  margin-bottom:0px;\n"
"}\n"
".mginput {\n"
"  margin-bottom:1vh;\n"
"  .form-control;\n"
"}\n"
;

const char templatea1[] PROGMEM =
"<!doctype html>\n"
"<html lang=\"en\">\n"
"  <head>\n"
"    <!-- Required meta tags -->\n"
"    <meta charset=\"utf-8\">\n"
"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1, shrink-to-fit=yes\">\n"
"    <!-- Bootstrap CSS -->\n"
"<link rel=\"stylesheet\" href=\"https://stackpath.bootstrapcdn.com/bootstrap/4.1.3/css/bootstrap.min.css\" integrity=\"sha384-MCw98/SFnGE8fJT3GXwEOngsV7Zt27NXFoaoApmYm81iuXoPkFOJwJ8ERdknLPMO\" crossorigin=\"anonymous\">\n"
"    <link rel=\"stylesheet\" type=\"text/css\" href=\"/custom.css\">\n"
"    <!-- Optional JavaScript -->\n"
"    <!-- jQuery first, then Popper.js, then Bootstrap JS -->\n"
;

const char templatea2[] PROGMEM = 
"      <script src=\"https://code.jquery.com/jquery-3.3.1.slim.min.js\" integrity=\"sha384-q8i/X+965DzO0rT7abK41JStQIAqVgRVzpbzo5smXKp4YfRvH+8abtTE1Pi6jizo\" crossorigin=\"anonymous\"></script>\n"
"      <script src=\"https://cdnjs.cloudflare.com/ajax/libs/popper.js/1.14.3/umd/popper.min.js\" integrity=\"sha384-ZMP7rVo3mIykV+2+9J3UJ46jBk0WLaUAdn689aCwoqbBJiSnjAK/l8WvCWPIPm49\" crossorigin=\"anonymous\"></script>\n"
"      <script src=\"https://stackpath.bootstrapcdn.com/bootstrap/4.1.3/js/bootstrap.min.js\" integrity=\"sha384-ChfqqxuZUCnJSK3+MXmPNIyE6ZbWh2IMqE241rYiqJxyMiZ6OW/JmZQ5stwEULTy\" crossorigin=\"anonymous\"></script>\n"
"      <script>\n"
"        $(function(){\n"
"          $(\".mglabel\").addClass(\"label\");\n"
"          $(\".mginput\").addClass(\"form-control\");\n"
"          $(\".mgbutton\").addClass(\"btn\");\n"
"          $(\".mgbutton\").addClass(\"btn-primary\");\n"
"        });\n"
"      </script>\n"
"  </body>\n"
"</html>\n"
;
