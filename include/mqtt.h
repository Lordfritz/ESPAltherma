#include <PubSubClient.h>
#include <EEPROM.h>
#include "restart.h"

#define MQTT_attr "espaltherma/ATTR"
#define MQTT_lwt "espaltherma/LWT"

#define EEPROM_CHK 1
#define EEPROM_STATE 0
#define EEPROM_SG 2

#define MQTT_attr "espaltherma/ATTR"
#define MQTT_lwt "espaltherma/LWT"

#ifdef JSONTABLE
char jsonbuff[MAX_MSG_SIZE] = "[{\0";
#else
char jsonbuff[MAX_MSG_SIZE] = "{\0";
#endif

#ifdef MQTT_ENCRYPTED
#include <WiFiClientSecure.h>
WiFiClientSecure espClient;
#else
WiFiClient espClient;
#endif
PubSubClient client(espClient);

void sendValues()
{
  Serial.printf("Sending values in MQTT.\n");
#ifdef ARDUINO_M5Stick_C_Plus2
  //Add Power values
  // getBatteryVoltage returns battery voltage [mV] as an int16_t
  float batteryVoltage = (float) M5.Power.getBatteryVoltage() / 1000; // convert to V as a float
  snprintf(jsonbuff + strlen(jsonbuff),MAX_MSG_SIZE - strlen(jsonbuff) , "\"%s\":\"%.3gV\",", "M5BatV", batteryVoltage);
#elif ARDUINO_M5Stick_C
  //Add M5 APX values
  snprintf(jsonbuff + strlen(jsonbuff),MAX_MSG_SIZE - strlen(jsonbuff) , "\"%s\":\"%.3gV\",\"%s\":\"%gmA\",", "M5VIN", M5.Axp.GetVinVoltage(),"M5AmpIn", M5.Axp.GetVinCurrent());
  snprintf(jsonbuff + strlen(jsonbuff),MAX_MSG_SIZE - strlen(jsonbuff) , "\"%s\":\"%.3gV\",\"%s\":\"%gmA\",", "M5BatV", M5.Axp.GetBatVoltage(),"M5BatCur", M5.Axp.GetBatCurrent());
  snprintf(jsonbuff + strlen(jsonbuff),MAX_MSG_SIZE - strlen(jsonbuff) , "\"%s\":\"%.3gmW\",", "M5BatPwr", M5.Axp.GetBatPower());
#endif
  snprintf(jsonbuff + strlen(jsonbuff),MAX_MSG_SIZE - strlen(jsonbuff) , "\"%s\":\"%ddBm\",", "WifiRSSI", WiFi.RSSI());
  snprintf(jsonbuff + strlen(jsonbuff),MAX_MSG_SIZE - strlen(jsonbuff) , "\"%s\":\"%d\",", "FreeMem", ESP.getFreeHeap());
  jsonbuff[strlen(jsonbuff) - 1] = '}';
#ifdef JSONTABLE
  strcat(jsonbuff,"]");
#endif
  client.publish(MQTT_attr, jsonbuff);
#ifdef JSONTABLE
  strcpy(jsonbuff, "[{\0");
#else
  strcpy(jsonbuff, "{\0");
#endif
}

void digitalWriteSgPins(uint8_t state)
{
  if (state == 0)
  {
    // Set SG 0 mode => SG1 = INACTIVE, SG2 = INACTIVE
    digitalWrite(PIN_SG1, SG_RELAY_INACTIVE_STATE);
    digitalWrite(PIN_SG2, SG_RELAY_INACTIVE_STATE);
  }
  else if (state == 1)
  {
    // Set SG 1 mode => SG1 = INACTIVE, SG2 = ACTIVE
    digitalWrite(PIN_SG1, SG_RELAY_INACTIVE_STATE);
    digitalWrite(PIN_SG2, SG_RELAY_ACTIVE_STATE);
  }
  else if (state == 2)
  {
    // Set SG 2 mode => SG1 = ACTIVE, SG2 = INACTIVE
    digitalWrite(PIN_SG1, SG_RELAY_ACTIVE_STATE);
    digitalWrite(PIN_SG2, SG_RELAY_INACTIVE_STATE);
  }
  else if (state == 3)
  {
    // Set SG 3 mode => SG1 = ACTIVE, SG2 = ACTIVE
    digitalWrite(PIN_SG1, SG_RELAY_ACTIVE_STATE);
    digitalWrite(PIN_SG2, SG_RELAY_ACTIVE_STATE);
  }
}

void saveSgState(uint8_t state)
{
    EEPROM.write(EEPROM_SG, state);
    EEPROM.commit();
}

void saveThermState(uint8_t state)
{
    EEPROM.write(EEPROM_STATE, state);
    EEPROM.commit();
}

void restoreEEPROM()
{
  if ('R' == EEPROM.read(EEPROM_CHK)) {
    #ifdef PIN_THERM
      mqttSerial.printf("Restoring previous state: %s", (EEPROM.read(EEPROM_STATE) == THERM_RELAY_ACTIVE_STATE) ? "On" : "Off");
      digitalWrite(PIN_THERM, EEPROM.read(EEPROM_STATE));
    #endif
    #ifdef PIN_SG1
      digitalWriteSgPins(EEPROM.read(EEPROM_SG));
    #endif
  }
  else
  {
    mqttSerial.printf("EEPROM not initialized (%d). Initializing...", EEPROM.read(EEPROM_CHK));
    EEPROM.write(EEPROM_CHK, 'R');
    EEPROM.commit();
    #ifdef PIN_THERM
      saveThermState(THERM_RELAY_INACTIVE_STATE);
      digitalWrite(PIN_THERM, THERM_RELAY_INACTIVE_STATE);
    #endif
    #ifdef PIN_SG1
      saveSgState(0);
      digitalWriteSgPins(0);
    #endif
  }
}

void reconnectMqtt()
{
  // Loop until we're reconnected
  int i = 0;
  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");

    if (client.connect("ESPAltherma-dev", MQTT_USERNAME, MQTT_PASSWORD, MQTT_lwt, 0, true, "Offline"))
    {
      Serial.println("connected!");
      #ifdef MQTT_HA_DISCOVERY
        client.publish("homeassistant/sensor/espAltherma/AlthermaSensors/config", "{\"availability\":[{\"topic\":\"espaltherma/LWT\",\"payload_available\":\"Online\",\"payload_not_available\":\"Offline\"}],\"availability_mode\":\"all\",\"unique_id\":\"espaltherma_sensors\",\"device\":{\"identifiers\":[\"ESPAltherma\"],\"manufacturer\":\"ESPAltherma\",\"model\":\"M5StickC PLUS ESP32-PICO\",\"name\":\"ESPAltherma\"},\"icon\":\"mdi:access-point-check\",\"name\":\"ESPAltherma Sensors\",\"state_topic\":\"espaltherma/LWT\",\"json_attributes_topic\":\"espaltherma/ATTR\"}", true);
        #ifdef PIN_THERM
          client.publish("homeassistant/switch/espAltherma/switch/config", "{\"availability\":[{\"topic\":\"espaltherma/LWT\",\"payload_available\":\"Online\",\"payload_not_available\":\"Offline\"}],\"availability_mode\":\"all\",\"unique_id\":\"espaltherma_switch\",\"device\":{\"identifiers\":[\"ESPAltherma\"],\"manufacturer\":\"ESPAltherma\",\"model\":\"M5StickC PLUS ESP32-PICO\",\"name\":\"ESPAltherma\"},\"icon\":\"mdi:water-boiler\",\"name\":\"EspAltherma Heat Pump Demand\",\"command_topic\":\"espaltherma/POWER\",\"state_topic\":\"espaltherma/STATE\",\"payload_on\":\"ON\",\"payload_off\":\"OFF\"}", true);
          // Subscribe
          client.subscribe("espaltherma/POWER");
        #endif
      #endif
      client.publish(MQTT_lwt, "Online", true);

      #ifdef PIN_SG1
        // Smart Grid
        #ifdef MQTT_HA_DISCOVERY
          client.publish("homeassistant/select/espAltherma/sg/config", "{\"availability\":[{\"topic\":\"espaltherma/LWT\",\"payload_available\":\"Online\",\"payload_not_available\":\"Offline\"}],\"availability_mode\":\"all\",\"unique_id\":\"espaltherma_sg\",\"device\":{\"identifiers\":[\"ESPAltherma\"],\"manufacturer\":\"ESPAltherma\",\"model\":\"M5StickC PLUS ESP32-PICO\",\"name\":\"ESPAltherma\"},\"icon\":\"mdi:solar-power\",\"name\":\"EspAltherma Smart Grid\",\"command_topic\":\"espaltherma/sg/set\",\"command_template\":\"{% if value == 'Free Running' %} 0 {% elif value == 'Forced Off' %} 1 {% elif value == 'Recommended On' %} 2 {% elif value == 'Forced On' %} 3 {% else %} 0 {% endif %}\",\"options\":[\"Free Running\",\"Forced Off\",\"Recommended On\",\"Forced On\"],\"state_topic\":\"espaltherma/sg/state\",\"value_template\":\"{% set mapper = { '0':'Free Running', '1':'Forced Off', '2':'Recommended On', '3':'Forced On' } %} {% set word = mapper[value] %} {{ word }}\"}", true);
        #endif
        client.subscribe("espaltherma/sg/set");
        char state[1];
        sprintf(state, "%d", EEPROM.read(EEPROM_SG));
        client.publish("espaltherma/sg/state", state, true);
      #endif

      #ifdef SAFETY_RELAY_PIN
        // Safety relay
        #ifdef MQTT_HA_DISCOVERY
          client.publish("homeassistant/switch/espAltherma/safety/config", "{\"availability\":[{\"topic\":\"espaltherma/LWT\",\"payload_available\":\"Online\",\"payload_not_available\":\"Offline\"}],\"availability_mode\":\"all\",\"unique_id\":\"espaltherma_safety\",\"device\":{\"identifiers\":[\"ESPAltherma\"],\"manufacturer\":\"ESPAltherma\",\"model\":\"M5StickC PLUS ESP32-PICO\",\"name\":\"ESPAltherma\"},\"icon\":\"mdi:water-boiler\",\"name\":\"EspAltherma Safety\",\"command_topic\":\"espaltherma/SAFETY\",\"state_topic\":\"espaltherma/SAFETY_STATE\",\"payload_on\":\"ON\",\"payload_off\":\"OFF\"}", true);
        #endif
        client.subscribe("espaltherma/SAFETY");
      #endif

      #ifdef PIN_PULSE
        // Pulse Meter
        #ifdef MQTT_HA_DISCOVERY
          client.publish("homeassistant/number/espAltherma/pulse/config", "{\"availability\":[{\"topic\":\"espaltherma/LWT\",\"payload_available\":\"Online\",\"payload_not_available\":\"Offline\"}],\"availability_mode\":\"all\",\"unique_id\":\"espaltherma_grid\",\"device\":{\"identifiers\":[\"ESPAltherma\"],\"manufacturer\":\"ESPAltherma\",\"model\":\"M5StickC PLUS ESP32-PICO\",\"name\":\"ESPAltherma\"},\"icon\":\"mdi:meter-electric\",\"name\":\"EspAltherma Power Limitation\",\"min\":0,\"max\":90000,\"mode\":\"box\",\"unit_of_measurement\":\"W\",\"command_topic\":\"espaltherma/pulse/set\",\"state_topic\":\"espaltherma/pulse/state\"}", true);
          client.subscribe("espaltherma/pulse/set");
          client.publish("espaltherma/pulse/state", "0");
        #endif
      #endif

      #ifndef PIN_THERM
        #ifdef MQTT_HA_DISCOVERY
          client.publish("homeassistant/switch/espAltherma/switch/config", "", true);
        #endif
      #endif

      #ifndef PIN_SG1
        #ifdef MQTT_HA_DISCOVERY
          // Publish empty retained message so discovered entities are removed from HA
          client.publish("homeassistant/select/espAltherma/sg/config", "", true);
        #endif
      #endif

      #ifndef PIN_PULSE
        #ifdef MQTT_HA_DISCOVERY
          // Publish empty retained message so discovered entities are removed from HA
          client.publish("homeassistant/select/espAltherma/pulse/config", "", true);
        #endif
      #endif
    }
    else
    {
      Serial.printf("failed, rc=%d, try again in 5 seconds", client.state());
      unsigned long start = millis();
      while (millis() < start + 5000)
      {
        ArduinoOTA.handle();
      }

      if (i++ == 100) {
        Serial.printf("Tried for 500 sec, rebooting now.");
        restart_board();
      }
    }
  }
}

void callbackTherm(byte *payload, unsigned int length)
{
  payload[length] = '\0';

  // Is it ON or OFF?
  // Ok I'm not super proud of this, but it works :p
  if (payload[1] == 'F')
  { //turn off
    #ifdef PIN_THERM
      digitalWrite(PIN_THERM, THERM_RELAY_INACTIVE_STATE);
      saveThermState(THERM_RELAY_INACTIVE_STATE);
      client.publish("espaltherma/STATE", "OFF", true);
      mqttSerial.println("Turned OFF");
    #endif
  }
  else if (payload[1] == 'N')
  { //turn on
    #ifdef PIN_THERM
      digitalWrite(PIN_THERM, THERM_RELAY_ACTIVE_STATE);
      saveThermState(THERM_RELAY_ACTIVE_STATE);
      client.publish("espaltherma/STATE", "ON", true);
      mqttSerial.println("Turned ON");
    #endif
  }
  else if (payload[0] == 'R') //R(eset/eboot)
  {
    mqttSerial.println("Rebooting");
    delay(100);
    restart_board();
  }
  else
  {
    Serial.printf("Unknown message: %s\n", payload);
  }
}

#ifdef PIN_SG1
//Smartgrid callbacks
void callbackSg(byte *payload, unsigned int length)
{
  payload[length] = '\0';

  if (payload[0] == '0')
  {
    // Set SG 0 mode => SG1 = INACTIVE, SG2 = INACTIVE
    digitalWriteSgPins(0);
    saveSgState(0);
    client.publish("espaltherma/sg/state", "0", true);
    Serial.println("Set SG mode to 0 - Normal operation");
  }
  else if (payload[0] == '1')
  {
    // Set SG 1 mode => SG1 = INACTIVE, SG2 = ACTIVE
    digitalWriteSgPins(1);
    saveSgState(1);
    client.publish("espaltherma/sg/state", "1", true);
    Serial.println("Set SG mode to 1 - Forced OFF");
  }
  else if (payload[0] == '2')
  {
    // Set SG 2 mode => SG1 = ACTIVE, SG2 = INACTIVE
    digitalWriteSgPins(2);
    saveSgState(2);
    client.publish("espaltherma/sg/state", "2", true);
    Serial.println("Set SG mode to 2 - Recommended ON");
  }
  else if (payload[0] == '3')
  {
    // Set SG 3 mode => SG1 = ACTIVE, SG2 = ACTIVE
    digitalWriteSgPins(3);
    saveSgState(3);
    client.publish("espaltherma/sg/state", "3", true);
    Serial.println("Set SG mode to 3 - Forced ON");
  }
  else
  {
    Serial.printf("Unknown message: %s\n", payload);
  }
}
#endif

#ifdef SAFETY_RELAY_PIN
void callbackSafety(byte *payload, unsigned int length)
{
  payload[length] = '\0';

  if (payload[0] == '0')
  {
    // Set Safety relay to OFF
    digitalWrite(SAFETY_RELAY_PIN, !SAFETY_RELAY_ACTIVE_STATE);
    client.publish("espaltherma/SAFETY_STATE", "0", true);
  }
  else if (payload[0] == '1')
  {
    // Set Safety relay to ON
    digitalWrite(SAFETY_RELAY_PIN, SAFETY_RELAY_ACTIVE_STATE);
    client.publish("espaltherma/SAFETY_STATE", "1", true);
  }
  else
  {
    Serial.printf("Unknown message: %s\n", payload);
  }
}
#endif

#ifdef PIN_PULSE
// time between pulses (excl. PULSE_DURATION_MS)
volatile double ms_until_pulse = 0;

// hardware timer pointer
hw_timer_t * timerPulseStart = NULL;
hw_timer_t * timerPulseEnd = NULL;

// hardware timer callback for when the pulse should start
void IRAM_ATTR onPulseStartTimer()
{
  #ifdef PULSE_LED_BUILTIN
    digitalWrite(LED_BUILTIN, HIGH);
  #endif
  digitalWrite(PIN_PULSE, HIGH);

  timerWrite(timerPulseEnd, 0);
  timerAlarmWrite(timerPulseEnd, PULSE_DURATION_MS * 1000, false);
  timerAlarmEnable(timerPulseEnd);
}

// hardware timer callback when the pulse duration is over
void IRAM_ATTR onPulseEndTimer()
{
  #ifdef PULSE_LED_BUILTIN
    digitalWrite(LED_BUILTIN, LOW);
  #endif
  digitalWrite(PIN_PULSE, LOW);

  timerWrite(timerPulseStart, 0);
  timerAlarmWrite(timerPulseStart, ms_until_pulse * 1000, false);
  timerAlarmEnable(timerPulseStart);
}

void setupPulseTimer()
{
  Serial.println("Setting up pulse timer");
  // Initilise the timer.
  // Parameter 1 is the timer we want to use. Valid: 0, 1, 2, 3 (total 4 timers)
  // Parameter 2 is the prescaler. The ESP32 default clock is at 80MhZ. The value "80" will
  // divide the clock by 80, giving us 1,000,000 ticks per second.
  // Parameter 3 is true means this counter will count up, instead of down (false).
  timerPulseStart = timerBegin(0, 80, true);
  timerPulseEnd = timerBegin(1, 80, true);

  // Attach the timer to the interrupt service routine named "onTimer".
  // The 3rd parameter is set to "true" to indicate that we want to use the "edge" type (instead of "flat").
  timerAttachInterrupt(timerPulseStart, &onPulseStartTimer, true);
  timerAttachInterrupt(timerPulseEnd, &onPulseEndTimer, true);

  // one tick is 1 micro second -> multiply msec by 1000
  timerAlarmWrite(timerPulseStart, ms_until_pulse * 1000, false);
  timerAlarmEnable(timerPulseStart);
}

// Pulse Meter callback
void callbackPulse(byte *payload, unsigned int length)
{
  payload[length] = '\0';
  String ss((char*)payload);
  long target_watt = ss.toInt();

  // also converts from kWh to Wh
  float WH_PER_PULSE = (1.0 / PULSES_PER_kWh) * 1000;

  ms_until_pulse = ((3600.0 / target_watt) * WH_PER_PULSE * 1000) - PULSE_DURATION_MS;
  if ((ms_until_pulse + PULSE_DURATION_MS) > 60 * 1000) {
    // cap the maximum pulse length to 1 minute
    // a change of the pulse is only applied, after the current pulse is finished. Thus if the pulse rate is very low,
    // it will take a long time to adjust the rate
    ms_until_pulse = 60 * 1000;
    target_watt = (long) WH_PER_PULSE * 60;
    Serial.printf("Capping pulse to %d Watt to ensure pulse rate is <= 60 sec\n", target_watt);
  }
  if (ms_until_pulse < 100) {
    // ensure a 100 ms gap between two pulses
    // taken from https://www.manualslib.de/manual/480757/Daikin-Brp069A61.html?page=10#manual
    ms_until_pulse = 100;
    long original_target = target_watt;
    target_watt = (long) ((1000 * WH_PER_PULSE * 3600) / (ms_until_pulse + PULSE_DURATION_MS));
    Serial.printf("WARNING pulse frequency to high, capping at %d Watt! Target is %d Watt. Decrease PULSE_DURATION or PULSES_PER_kWh\n", target_watt, original_target);
  }
  if (timerPulseStart == NULL) {
    setupPulseTimer();
  }
  client.publish("espaltherma/pulse/state", String(target_watt).c_str());
  Serial.printf("Set pulse meter to target %d Watt\n", target_watt);
}
#endif

void callback(char *topic, byte *payload, unsigned int length)
{
  Serial.printf("Message arrived [%s] : %s\n", topic, payload);

  if (strcmp(topic, "espaltherma/POWER") == 0)
  {
    callbackTherm(payload, length);
  }
#ifdef PIN_SG1
  else if (strcmp(topic, "espaltherma/sg/set") == 0)
  {
    callbackSg(payload, length);
  }
#endif
#ifdef SAFETY_RELAY_PIN
  else if (strcmp(topic, "espaltherma/SAFETY") == 0)
  {
    callbackSafety(payload, length);
  }
#endif
#ifdef PIN_PULSE
  else if (strcmp(topic, "espaltherma/pulse/set") == 0)
  {
    callbackPulse(payload, length);
  }
#endif

  else
  {
    Serial.printf("Unknown topic: %s\n", topic);
  }
}
