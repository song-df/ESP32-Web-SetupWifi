#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>

#define WIFI_PARAM_FILE "/root/wifi.txt"
AsyncWebServer server(80); 
File file; 
StaticJsonDocument<20000> doc; 
String content;
String SSID = "xxx";                                                                   // ssid : WIFI名称
String PASSWORD = "xxx";  

void FileSystemInit()
{
  //文件系统挂载
  if (SPIFFS.begin(true, "/root", 3))
  {
    Serial.println("File System is initialized");
    Serial.print("Successfully mounts in /root \nFile system now used is ");
    //Serial.println(SPIFFS.totalBytes()); //文件系统总大小
    Serial.print(SPIFFS.usedBytes());  //文件系统已使用
    Serial.println("B.");
  }
  else{
    Serial.println("An error occurred while mounting SPIFFS");
  }
}
void webroot(AsyncWebServerRequest *request)
{
    /*设置配网Web页面*/
  content = "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><title>WIFI配置</title></head><body>";
  content += "<h1 align=\"center\">Connect device to your WiFi</h1>";
  content += "<br><form action=\"/wifi\"><table align=\"center\" frame=\"box\">";
  content += "<tr><td><input type=\"radio\" name=\"method\" value=\"Auto\" checked onclick=\"Auto_wifi()\">扫描WIFI</td>";
  content += "<td><input type=\"radio\" name=\"method\" value=\"Manual\" onclick=\"Manual_wifi()\">手动输入</td></tr>";
  content += "<tr id=\"scan\"><td align=\"right\">SSID:</td><td><select name=\"ssid\" style=\"width:200px\">";
  content += "<option selected>Select a Network</option>";
  /*获取热点数量*/

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
  int WIFI_number = WiFi.scanNetworks();
  /*判断有没有扫描到周围热点*/
  if (WIFI_number != 0)
  {
    for (int i = 1; i < WIFI_number; i++)
    {
      /*配置Web表单热点所选项*/
      content += "<option value=\"" + WiFi.SSID(i) + "\">" + WiFi.SSID(i) + "</option>";
      delay(100);
    }
    content += "</select></td></tr>";
    content += "<tr id=\"Manual\"><td align=\"right\">SSID:</td><td><input name=\"ssid_manual\" style=\"width:200px\"></td></tr>";
    content += "<tr><td align=\"right\">Password:</td><td><input name=\"password\" style=\"width:200px\" ></td><tr>";
    content += "<tr><td></td><td align=\"right\"><button type=\"submit\" style=\"width:100px\">开始配置</button></td></tr></table></form></body>";
    content += "<script>document.getElementById(\"Manual\").style.display = \"none\";function Auto_wifi() {";
    content += "document.getElementById(\"scan\").style.display = \"table-row\";";
    content += "document.getElementById(\"Manual\").style.disable = true;";
    content += "document.getElementById(\"Manual\").style.display = \"none\";";
    content += "window.location.reload();}";
    content += "function Manual_wifi() {";
    content += "document.getElementById(\"scan\").style.display = \"none\";";
    content += "document.getElementById(\"scan\").style.disable = true;";
    content += "document.getElementById(\"Manual\").style.display = \"table-row\";";
    content += "}</script></html>";

    WiFi.scanDelete(); //清除扫描数据
    Serial.println(content);
    /*Web服务器返回状态码200,以及配置WiFi页面*/
    request->send(200, "text/html", content);
  }
  else
  {
    content = "<h1 algin=\"center\">The server is busy, please refresh the page.</h1>";
    request->send(200, "text/html", content);
  }
}
void webwifi(AsyncWebServerRequest *request)
{
  /*WIFI扫描配网*/
  if (request->arg("method") == "Auto")
  {
    SSID = request->arg("ssid");
    PASSWORD = request->arg("password");
    Serial.println("==========Auto===========");
  }
  /*WIFI手动配网*/
  if (request->arg("method") == "Manual")
  {
    SSID = request->arg("ssid_manual");
    PASSWORD = request->arg("password");
    Serial.println("==========manual===========");
  }
  char buffer[64];                                  //临时变量 存放json字符串
  doc["ssid"] = SSID;                               //添加成员ssid
  doc["password"] = PASSWORD;                       //添加成员password
  serializeJson(doc, buffer);                       //格式化JSON 转存到 buffer
  file = SPIFFS.open(WIFI_PARAM_FILE, FILE_WRITE); //以写的方式打开wifi.txt文件,每次覆盖之前,没有就创建
  file.write((uint8_t *)buffer, strlen(buffer));    //把buffer里面的写入到wifi.txt
  file.close();                                     //关闭文件

  /*Web服务器返回状态码200,提示配网和Web服务器结束*/
  request->send(200, "text/html", "<h1 align=\"center\">Successfully connected to wifi</h1><br><h1 align=\"center\">Web servers will be shut down</h1>");
  
  /*关闭Web服务器,重新连接wifi*/
  server.end();
  Serial.println("will reboot ESP32...");
  delay(5000);

  /*重启ESP32*/
  ESP.restart();
}
void webreset(AsyncWebServerRequest *request)
{
  request->send(200, "text/html", "<h1 align=\"center\">ESP32S3 will reset, do not refresh this page, will cause ESP32 reset again!!!</h1>");
  /*关闭Web服务器,重启ESP32*/
  server.end();

  Serial.println("will reboot ESP32...");
  delay(3000);
  /*重启ESP32*/
  ESP.restart();
}
/*
 * @brief:Web配网
 * @param:none
 * @retval:none
*/
void web_setupwifi()
{
  /*设置wifi为 AP热点*/
  WiFi.mode(WIFI_AP);
  /*设置热点WIFI*/
  WiFi.softAP("ESP32_AP", "");
  /*获取热点IP地址*/
  IPAddress myIP = WiFi.softAPIP();
  /*串口显示热点IP地址*/
  Serial.print("AP IP address: ");
  Serial.println(myIP);
  /*设置服务器监听 url 以及 服务器回应(回调函数) */
  server.on("/", webroot);
  server.on("/wifi", webwifi);
  server.on("/reset",webreset);
  /*设置服务器端口 并且开启*/
  server.begin();
}
void wifi_connect()
{
  //设置wifi station模式
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  /*判断是否存在wifi.txt文件*/
  if (SPIFFS.exists(WIFI_PARAM_FILE))
  {
    Serial.println("find /root/wifi.txt");
    /*以只读的方式打开wifi.txt文件*/
    file = SPIFFS.open(WIFI_PARAM_FILE, FILE_READ);
    /*定义临时变量存储wifi.txt的内容*/
    String Temp = file.readString();
    Serial.println(Temp);
    /*关闭文件*/
    file.close();
    /*对字符串解包生成所需要的json数据*/
    DeserializationError error = deserializeJson(doc, Temp);
    if (error)
    {
      /*格式化失败*/
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.c_str());
      Serial.println("ESP32 will reboot...");
      SPIFFS.remove(WIFI_PARAM_FILE);
      if(!SPIFFS.exists(WIFI_PARAM_FILE))
        Serial.printf("%s removed.\n",WIFI_PARAM_FILE);
      
      delay(5000);
      ESP.restart(); //重启
    }
    else
    {
      /*格式化成功*/
      /*取出变量ssid & password的值*/
      String temp_ssid = doc["ssid"];
      String temp_pwd = doc["password"];
      /*赋值给 SSID & PASSWORD */
      SSID = temp_ssid;
      PASSWORD = temp_pwd;
      //Serial.println("ssid: " + SSID);
      //Serial.println("password: " + PASSWORD);
    }
  }
  /*WIFI连接*/
  WiFi.begin(SSID.c_str(), PASSWORD.c_str());
  /*Blinker 连接上 及确保WIFI连接上*/
  //Blinker.begin(auth.c_str(), SSID.c_str(), PASSWORD.c_str());
  //判断连接状态
  int count = 0;
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    if (count >= 5)
    {
      /*超时5s 结束程序*/
      break;
    }
    Serial.println("WIFI is connecting ...");
    count++;
  }

  if (WiFi.isConnected())
  {
    Serial.println("WIFI connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP().toString());
  }
  else
  {
    /*进入Web配网*/
    Serial.println("Web配网.....");
    web_setupwifi();
  }
}
void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  
  FileSystemInit();

  wifi_connect();

  server.on("/reset", webreset);
  /*设置服务器端口 并且开启*/
  server.begin();

#ifdef TEST_FILE_ACCESS

  if(!SPIFFS.exists(WIFI_PARAM_FILE)){
    

    file = SPIFFS.open(WIFI_PARAM_FILE, FILE_WRITE); //以写的方式打开wifi.txt文件,每次覆盖之前,没有就创建
    if (!file) {
      Serial.println("Failed to open file for writing");
      return;
    }
    file.write((uint8_t *)"hello write to wifi.txt", strlen("hello write to wifi.txt"));    //把buffer里面的写入到wifi.txt
    file.close(); 
  }
  else{
    file = SPIFFS.open(WIFI_PARAM_FILE, FILE_READ);
    if (!file) {
      Serial.println("Failed to open file for reading");
      return;
    }
    String Temp = file.readString();
    file.close();
    Serial.println(Temp);
  }
  #endif
}

void loop() {
  // put your main code here, to run repeatedly:
}
