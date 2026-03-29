/*
 * ESP8266 智能车库 WiFi/MQTT 网关
 * 功能：通过串口与 STM32 通信，转发数据到 MQTT 服务器，接收远程控制命令
 * 硬件：NodeMCU 1.0 (ESP-12E)
 * 连接：D5(RX) <-> STM32 PA2(TX), D6(TX) <-> STM32 PA3(RX)
 */

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <SoftwareSerial.h>

// ==================== 配置区域 ====================

// WiFi 配置 - 请修改为你的网络信息
const char* WIFI_SSID = "你的WiFi名称";
const char* WIFI_PASSWORD = "你的WiFi密码";

// MQTT 服务器配置 - 请修改为你的电脑 IP
const char* MQTT_SERVER = "192.168.1.100";  // 运行 Mosquitto 的电脑 IP
const int MQTT_PORT = 1883;

// MQTT 主题
const char* TOPIC_STATUS_TEMP = "garage/status/temperature";
const char* TOPIC_STATUS_HUMI = "garage/status/humidity";
const char* TOPIC_STATUS_SMOKE = "garage/status/smoke";
const char* TOPIC_STATUS_DISTANCE = "garage/status/distance";
const char* TOPIC_STATUS_DOOR = "garage/status/door";
const char* TOPIC_STATUS_ALL = "garage/status/all";
const char* TOPIC_ALARM = "garage/alarm";
const char* TOPIC_CONTROL_DOOR = "garage/control/door";
const char* TOPIC_CONTROL_CMD = "garage/control/cmd";

// 软件串口配置 (与 STM32 通信)
#define STM32_RX D5  // 接 STM32 PA2 (TX)
#define STM32_TX D6  // 接 STM32 PA3 (RX)
SoftwareSerial stm32Serial(STM32_RX, STM32_TX);

// ==================== 全局变量 ====================

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

// 传感器数据缓存
float lastTemp = 0;
float lastHumi = 0;
uint8_t lastSmokeLevel = 0;
uint16_t lastSmokePpm = 0;
uint16_t lastDistance = 999;
bool lastDoorOpen = false;

// 状态标志
bool wifiConnected = false;
bool mqttConnected = false;
unsigned long lastReconnectAttempt = 0;
unsigned long lastHeartbeat = 0;

// ==================== 初始化 ====================

void setup() {
    Serial.begin(115200);
    Serial.println("\n\nESP8266 Smart Garage Gateway");
    Serial.println("============================");
    
    // 初始化软件串口 (与 STM32 通信)
    stm32Serial.begin(115200);
    Serial.println("SoftwareSerial started @ 115200 baud");
    
    // 连接 WiFi
    setupWiFi();
    
    // 配置 MQTT
    mqtt.setServer(MQTT_SERVER, MQTT_PORT);
    mqtt.setCallback(mqttCallback);
    
    Serial.println("Setup complete, entering main loop...");
}

// ==================== WiFi 连接 ====================

void setupWiFi() {
    delay(10);
    Serial.print("Connecting to WiFi: ");
    Serial.println(WIFI_SSID);
    
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        Serial.println("\nWiFi connected!");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("\nWiFi connection failed!");
    }
}

// ==================== MQTT 连接 ====================

bool reconnectMQTT() {
    if (!wifiConnected) {
        setupWiFi();
    }
    
    String clientId = "ESP8266-Garage-" + String(ESP.getChipId(), HEX);
    
    Serial.print("Attempting MQTT connection as ");
    Serial.print(clientId);
    Serial.print("...");
    
    if (mqtt.connect(clientId.c_str())) {
        mqttConnected = true;
        Serial.println("connected!");
        
        // 订阅控制主题
        mqtt.subscribe(TOPIC_CONTROL_DOOR);
        mqtt.subscribe(TOPIC_CONTROL_CMD);
        Serial.println("Subscribed to control topics");
        
        // 发布上线通知
        mqtt.publish("garage/status/online", "1");
        return true;
    } else {
        mqttConnected = false;
        Serial.print("failed, rc=");
        Serial.println(mqtt.state());
        return false;
    }
}

// ==================== MQTT 消息回调 ====================

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    String topicStr = String(topic);
    String msg = "";
    for (unsigned int i = 0; i < length; i++) {
        msg += (char)payload[i];
    }
    
    Serial.print("[MQTT RX] ");
    Serial.print(topicStr);
    Serial.print(": ");
    Serial.println(msg);
    
    // 处理车库门控制
    if (topicStr == TOPIC_CONTROL_DOOR) {
        if (msg == "OPEN") {
            sendCommandToSTM32("CMD:DOOR:OPEN\n");
            Serial.println("-> Forwarded to STM32: DOOR OPEN");
        } else if (msg == "CLOSE") {
            sendCommandToSTM32("CMD:DOOR:CLOSE\n");
            Serial.println("-> Forwarded to STM32: DOOR CLOSE");
        }
    }
    
    // 处理通用命令
    if (topicStr == TOPIC_CONTROL_CMD) {
        if (msg == "GET_STATUS") {
            sendCommandToSTM32("CMD:GET_STATUS\n");
        }
    }
}

// ==================== STM32 通信 ====================

void sendCommandToSTM32(const char* cmd) {
    stm32Serial.print(cmd);
}

void processSTM32Data(String data) {
    data.trim();
    if (data.length() == 0) return;
    
    Serial.print("[STM32 RX] ");
    Serial.println(data);
    
    // 处理数据上报格式: DATA:temp,humidity,smoke,ppm,distance,door
    if (data.startsWith("DATA:")) {
        String values = data.substring(5); // 去掉 "DATA:"
        
        // 解析逗号分隔的值
        int idx1 = values.indexOf(',');
        int idx2 = values.indexOf(',', idx1 + 1);
        int idx3 = values.indexOf(',', idx2 + 1);
        int idx4 = values.indexOf(',', idx3 + 1);
        int idx5 = values.indexOf(',', idx4 + 1);
        
        if (idx1 > 0 && idx2 > idx1 && idx3 > idx2 && idx4 > idx3 && idx5 > idx4) {
            float temp = values.substring(0, idx1).toFloat();
            float humi = values.substring(idx1 + 1, idx2).toFloat();
            int smoke = values.substring(idx2 + 1, idx3).toInt();
            int ppm = values.substring(idx3 + 1, idx4).toInt();
            int dist = values.substring(idx4 + 1, idx5).toInt();
            String door = values.substring(idx5 + 1);
            
            // 更新缓存
            lastTemp = temp;
            lastHumi = humi;
            lastSmokeLevel = smoke;
            lastSmokePpm = ppm;
            lastDistance = dist;
            lastDoorOpen = (door == "OPEN");
            
            // 发布到 MQTT (如果已连接)
            if (mqttConnected) {
                mqtt.publish(TOPIC_STATUS_TEMP, String(temp, 1).c_str());
                mqtt.publish(TOPIC_STATUS_HUMI, String(humi, 1).c_str());
                mqtt.publish(TOPIC_STATUS_SMOKE, String(smoke).c_str());
                mqtt.publish(TOPIC_STATUS_DISTANCE, String(dist).c_str());
                mqtt.publish(TOPIC_STATUS_DOOR, door.c_str());
                
                // 发布 JSON 格式的完整状态
                String json = "{\"temp\":" + String(temp, 1) + 
                             ",\"humi\":" + String(humi, 1) +
                             ",\"smoke\":" + String(smoke) +
                             ",\"ppm\":" + String(ppm) +
                             ",\"dist\":" + String(dist) +
                             ",\"door\":\"" + door + "\"}";
                mqtt.publish(TOPIC_STATUS_ALL, json.c_str());
            }
        }
    }
    
    // 处理报警格式: ALARM:TYPE
    else if (data.startsWith("ALARM:")) {
        String alarmType = data.substring(6);
        Serial.print("!!! ALARM: ");
        Serial.println(alarmType);
        
        if (mqttConnected) {
            mqtt.publish(TOPIC_ALARM, alarmType.c_str());
        }
    }
    
    // 处理命令确认: ACK:CMD
    else if (data.startsWith("ACK:")) {
        Serial.println("Command acknowledged by STM32");
    }
}

// ==================== 主循环 ====================

void loop() {
    unsigned long now = millis();
    
    // 检查 MQTT 连接
    if (!mqtt.connected()) {
        mqttConnected = false;
        if (now - lastReconnectAttempt > 5000) {
            lastReconnectAttempt = now;
            if (reconnectMQTT()) {
                lastReconnectAttempt = 0;
            }
        }
    } else {
        mqtt.loop();
    }
    
    // 读取 STM32 发来的数据
    while (stm32Serial.available()) {
        String data = stm32Serial.readStringUntil('\n');
        processSTM32Data(data);
    }
    
    // 心跳包 (每 30 秒)
    if (mqttConnected && now - lastHeartbeat > 30000) {
        lastHeartbeat = now;
        mqtt.publish("garage/status/heartbeat", String(now).c_str());
    }
}
