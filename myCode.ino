 // A bunch of lines of code, designed to examine the master/branch/pull capabilities of Github.

// This is code that was added in the Mac local repo, which I will pull into the
// master on Github

// Some new code added
 
// Latest editing activity, designed to test myNewBranch

// Used Edit direct from Master branch, will now try to save and create new Branch and Pull request

#ifdef USE_I2C
#ifdef USE_SHT3X
/*********************************************************************************************\
 * SHT3X - Temperature and Humidy
 *
 * I2C Address: 0x44 or 0x45
\*********************************************************************************************/

#define SHT3X_ADDR_GND      0x44       // address pin low (GND)
#define SHT3X_ADDR_VDD      0x45       // address pin high (VDD)

uint8_t sht3x_count = 0;                    // since init to 0 on first entry, presumably "device found" indicator
uint8_t sht3x_address_list[] = { 0, 0 };    // potentially two devices on the I2C bus
uint8_t sht3x_addresses[] = { SHT3X_ADDR_GND, SHT3X_ADDR_VDD };

bool Sht3xRead(float &t, float &h, uint8_t sht3x_address) // Parameterised for device address
{
  unsigned int data[6];

  t = NAN;
  h = NAN;

  Wire.beginTransmission(sht3x_address);
  Wire.write(0x2C);                    // Enable clock stretching
  Wire.write(0x06);                    // High repeatability
  if (Wire.endTransmission() != 0) {   // Stop I2C transmission
    return false;
  }
  delay(30);                           // Timing verified with logic analyzer (10 is to short)
  Wire.requestFrom(sht3x_address, (uint8_t)6);   // Request 6 bytes of data
  for (int i = 0; i < 6; i++) {
    data[i] = Wire.read();             // cTemp msb, cTemp lsb, cTemp crc, humidity msb, humidity lsb, humidity crc
  };
  t = ConvertTemp((float)((((data[0] << 8) | data[1]) * 175) / 65535.0) - 45);
  h = (float)((((data[3] << 8) | data[4]) * 100) / 65535.0);
  return (!isnan(t) && !isnan(h));
}

/********************************************************************************************/

void Sht3xDetect()
{
  if (sht3x_count) {
    return;
  }

  float t;
  float h;
  for (byte i = 0; i < sizeof(sht3x_addresses); i++) {
    if (Sht3xRead(t, h, sht3x_addresses[i])) {
      sht3x_address_list[sht3x_count] = sht3x_addresses[i];
      snprintf_P(log_data, sizeof(log_data), S_LOG_I2C_FOUND_AT, "SHT3X", sht3x_address_list[sht3x_count]);
      AddLog(LOG_LEVEL_DEBUG);
      sht3x_count++;
    }
  }
}

void Sht3xShow(boolean json)
{
  /* 
    Modified to support multiple devices on a single I2C bus
    If there is only one device, do exactly what it did before, including all existing message formats
    Otherwise, use revised message formats to include both sensors' data in a single message  
  */
  if (sht3x_count) {
    bool valid_read[sizeof(sht3x_addresses)];
    bool at_least_one_valid = false;
    float t[sizeof(sht3x_addresses)];
    float h[sizeof(sht3x_addresses)];
    for (byte i = 0; i < sht3x_count; i++) {       // Check all the detected sensors
      if (valid_read[i] = Sht3xRead(t[i], h[i], sht3x_address_list[i]))
        at_least_one_valid = true;
    }   // read each device
  /*
    Now have array of t, h, valid read. 
    If only 1 device, and valid, use existing formats. 
    If 2 devices, use new formats, and if not valid read for one or more, substitute values
    The formatting formats into mqtt_data, including the current contents of mqtt_data as a %s in the first position
    Current JSON format:
    {"Time":"2018-02-18T15:42:04",
     "SHT3X":{"Temperature":20.6,"Humidity":45.0},
     "TempUnit":"C"
    }
    Proposed JSON for new format:
    {"Time": "2018-02-18T15:07:04",  
     "SHT3X":[
       {"Address": "0x44",  "Temperature": 20.7, "Humidity": 44.1 },
       {"Address": "0x45",  "Temperature": 20.7, "Humidity": 44.1 }
     ], 
     "TempUnit": "C"
    }
  */  
    if (at_least_one_valid) {
      char temperature[10];
      char humidity[10];
      // Let's keep this simple for now!
      if ((1 == sht3x_count)&&(valid_read[0])) { // Do exactly what we did before
        dtostrfd(t[0], Settings.flag2.temperature_resolution, temperature);
        dtostrfd(h[0], Settings.flag2.humidity_resolution, humidity);
        if (json) {
          snprintf_P(mqtt_data, sizeof(mqtt_data), JSON_SNS_TEMPHUM, mqtt_data, "SHT3X", temperature, humidity);
#ifdef USE_DOMOTICZ
          DomoticzTempHumSensor(temperature, humidity);
#endif  // USE_DOMOTICZ
#ifdef USE_WEBSERVER
        } else {
// const char HTTP_SNS_TEMP[] PROGMEM = "%s{s}%s " D_TEMPERATURE "{m}%s&deg;%c{e}"; // {s} = <tr><th>, {m} = </th><td>, {e} = </td></tr>
// const char HTTP_SNS_HUM[]  PROGMEM = "%s{s}%s " D_HUMIDITY "{m}%s%%{e}";          // {s} = <tr><th>, {m} = </th><td>, {e} = </td></tr>          
          snprintf_P(mqtt_data, sizeof(mqtt_data), HTTP_SNS_TEMP, mqtt_data, "SHT3X", temperature, TempUnit());
          snprintf_P(mqtt_data, sizeof(mqtt_data), HTTP_SNS_HUM, mqtt_data, "SHT3X", humidity);
#endif  // USE_WEBSERVER
        }
      } else { // more than one sensor
        if (json) {
          // Print the opening structure
          snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s,\"SHT3X\":["), mqtt_data);
        } // if json
        for (byte i = 0; i < sht3x_count; i++) {       // Print all the valid reads
          if (valid_read[i]) {
            dtostrfd(t[i], Settings.flag2.temperature_resolution, temperature);
            dtostrfd(h[i], Settings.flag2.humidity_resolution, humidity);
            if (json) {
              // We need {"Address":"0x44","Temperature":20.0,"Humidity":50.0},{...}
              if (i > 0) { // Need a separator (this is horrible!!)
                snprintf(mqtt_data, sizeof(mqtt_data),"%s,",mqtt_data);
              }
              snprintf_P(mqtt_data, sizeof(mqtt_data),\
                       PSTR("%s{\"" D_JSON_ADDRESS "\":\"0x%02x\",\"" D_JSON_TEMPERATURE "\":%s,\"" D_JSON_HUMIDITY "\":%s}"), \
                       mqtt_data, sht3x_address_list[i], temperature, humidity);
#ifdef USE_DOMOTICZ
              DomoticzTempHumSensor(temperature, humidity);
#endif  // USE_DOMOTICZ
#ifdef USE_WEBSERVER
            } else { // not JSON!!
              snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s{s}%s-0x%02x " D_TEMPERATURE "{m}%s&deg;%c{e}"), mqtt_data, "SHT3X", sht3x_address_list[i], temperature, TempUnit());
              snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s{s}%s-0x%02x " D_HUMIDITY "{m}%s%%{e}"), mqtt_data, "SHT3X", sht3x_address_list[i], humidity);
#endif  // USE_WEBSERVER
            }
          } // valid read for device
        }   // Format each device
        if (json) { // Print the clsing structure
          snprintf(mqtt_data, sizeof(mqtt_data),"%s]", mqtt_data); // Assumes TempUnit is added by caller?
        }
      }   // one or more devices    
    }     // at least one valid read     
  }       // any devices detected

// previous code - begin
/* 
  if (sht3x_type) {
    float t;
    float h;
    if (Sht3xRead(t, h)) {
      char temperature[10];
      char humidity[10];
      dtostrfd(t, Settings.flag2.temperature_resolution, temperature);
      dtostrfd(h, Settings.flag2.humidity_resolution, humidity);

      if (json) {
        snprintf_P(mqtt_data, sizeof(mqtt_data), JSON_SNS_TEMPHUM, mqtt_data, "SHT3X", temperature, humidity);
#ifdef USE_DOMOTICZ
        DomoticzTempHumSensor(temperature, humidity);
#endif  // USE_DOMOTICZ
#ifdef USE_WEBSERVER
      } else {
        snprintf_P(mqtt_data, sizeof(mqtt_data), HTTP_SNS_TEMP, mqtt_data, "SHT3X", temperature, TempUnit());
        snprintf_P(mqtt_data, sizeof(mqtt_data), HTTP_SNS_HUM, mqtt_data, "SHT3X", humidity);
#endif  // USE_WEBSERVER
      }
    }    
*/ 
// previous code - end

}

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

#define XSNS_14

boolean Xsns14(byte function)
{
  boolean result = false;

  if (i2c_flg) {
    switch (function) {
      case FUNC_INIT:
        Sht3xDetect();
        break;
      case FUNC_JSON_APPEND:
        Sht3xShow(1);
        break;
#ifdef USE_WEBSERVER
      case FUNC_WEB_APPEND:
        Sht3xShow(0);
        break;
#endif  // USE_WEBSERVER
    }
  }
  return result;
}

#endif  // USE_SHT3X
#endif  // USE_I2C
