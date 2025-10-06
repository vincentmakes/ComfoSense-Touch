# Zehnder Comfoair Q350 MQTT bridge

This software lets you use a ESP32 + CAN transceiver (and accesory components, total budget well under 20 € in parts) to interact with the Zehnder Comfoair Q350 (and probably any other Q-series) Mechanical Ventilation with Heat Recovery (MVHR) unit. Zehnder has an official hardware bridge called "Comfoconnect LAN C" which is not only dead expensive (over 250 €), but lacks most of the features this project provides.

Firmware running off the ESP32 does expose all known Zehnder Comfoair Q series metrics and status information through MQTT, and lets you control the air flow (and run other actions) via MQTT as well.

As such, it allows for a very easy integration into Home Assistant (see example Lovelace card below, not included in this repository). Note the Particulate Matter (PM) values shown in the screenshot below are obtained outside the Comfoair Q device) :
![Comfoair Q 350 Home Assistant](docs/homeassistant.png?raw=true "Comfoair Q 350 Home Assistant")


You can find the YAML configuration files for Home Assistant in the `docs` folder, which should allow you to :
- Have all metrics and MVHR unit state values published as sensors
- Easily configure a "entities" Lovelace card after creating an "input_select" Helper to set the ventilation level from the HA GUI


This repo is based off the original one ([vekexasia/comfoair-esp32](https://github.com/vekexasia/comfoair-esp32)) with a few changes, namely :
  * [PR #41](https://github.com/vekexasia/comfoair-esp32/pull/41) has been merged (although later changes I did for the most part override this)
  * Some original project issues (some of which are reported as Issues) have been fixed, ie [Issue #44](https://github.com/vekexasia/comfoair-esp32/issues/44)
  * Completely revamped, extended and up-to-date configuration files for MQTT sensors, templates and automations for Home Assistant


A huge thank you to the original author of this software, and to those who, in the first place, reversed-engineer the Zehnder Comfoair Q CAN bus protocol :

  * [A protocol adapter for Zehnder ComfoAir Q series devices with CAN bus interface](https://github.com/marco-hoyer/zcan/issues/1).
  * [ComfoControl CAN/RMI Protocol](https://github.com/michaelarnauts/comfoconnect/blob/master/PROTOCOL-RMI.md)
  * [comfoconnect/PROTOCOL-PDO.md](https://github.com/michaelarnauts/comfoconnect/blob/master/PROTOCOL-PDO.md)



## Hardware Components

Prerequisites:

* Any ESP32 development board (of course, your abilities permitting, will work with a discrete ESP32 board) -> [example](https://amzn.to/3pe0XVP), although any ESP32 WROOM development board off Aliexpress or similar should work ([example](https://es.aliexpress.com/item/1005004285144170.html))
* A DC-DC converter from 12 V to 3.3 V (fixed or configurable output voltage) -> [link](https://amzn.to/39ar22v)
* A CAN transceiver board -> [Waveshare SN65HVD230](https://www.banggood.com/Waveshare-SN65HVD230-CAN-Bus-Module-Communication-CAN-Bus-Transceiver-Development-Board-p-1693712.html?rmmds=myorder&cur_warehouse=CN), also can easily be sourced from Aliexpress (I chose a [smaller form-factor version](https://www.aliexpress.com/item/1005001868558551.html))
* Some ethernet cable (the lower the AWG, hence bigger the diameter, the better) or any other communications cable up to 0.75 mm2
* Any enclosure with enough room for your components, to keep them safe and tidy

The MVHR unit puts DC 12 V out which is to be fed to the DC / DC converter, which will provide 3.3 DC power to both the ESP32 development board and the CAN transceiver. Remaining two pins in the MVHR unit are for the CAN protocol, need be connected as described below to CAN transceiver, then to the corresponding GPIO ports in the ESP32 development board.


## Flashing the firmware in the ESP32 development board

First, create a "secrets.h" file at the top of this repository, with the configuration values below adapted to your environment (in summary, MQTT server and topic details and wireless SSID and passphrase) :

```c++
#define MQTT_HOST "192.168.x.y"
#define MQTT_PASS ""
#define MQTT_PORT 1883
#define MQTT_PREFIX "comfoair"
#define MQTT_USER ""
#define WIFI_SSID "YOUR_WIRELESS_NETWORK_SSID"
#define WIFI_PASS "WIRELESS_PASSWORD"
```

Next, connect the ESP32 development board to your PC using a USB cable, and it should be as easy as running the following command to get "platformio" to compile and upload the firmware (command output trimmed for brevity) :
```bash
# pio run --environment wemos_d1_mini32 --target upload
Processing wemos_d1_mini32 (platform: platformio/espressif32@3.5.0; board: esp32dev; framework: arduino)
----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
CONFIGURATION: https://docs.platformio.org/page/boards/espressif32/esp32dev.html
PLATFORM: Espressif 32 (3.5.0) > Espressif ESP32 Dev Module
HARDWARE: ESP32 240MHz, 320KB RAM, 4MB Flash
...
Linking .pio/build/wemos_d1_mini32/firmware.elf
Retrieving maximum program size .pio/build/wemos_d1_mini32/firmware.elf
Checking size .pio/build/wemos_d1_mini32/firmware.elf
Advanced Memory Usage is available via "PlatformIO Home > Project Inspect"
RAM:   [=         ]  12.9% (used 42216 bytes from 327680 bytes)
Flash: [======    ]  63.1% (used 826830 bytes from 1310720 bytes)
Building .pio/build/wemos_d1_mini32/firmware.bin
...
CURRENT: upload_protocol = esptool
Looking for upload port...
Using manually specified: /dev/ttyUSB0
Uploading .pio/build/wemos_d1_mini32/firmware.bin
esptool.py v3.1
Serial port /dev/ttyUSB0
```

You may now disconnect the ESP32 from the PC. The ESP32 should now be connected to WiFi (make sure it gets assigned a static IP address for convenience), but will not put any messages to MQTT until getting valid input signal from CAN transceiver. If in the future you want to upload an updated firmware version, this project supports OTA (over the air), so just :

  * Make whatever the changes you need to the source code (ie the values in secrets.h)
  * Recompile using the mentioned "pio" command above : no need to connect ESP32 board to USB
  * "pio" will fail to upload the firmware, as the ESP32 is not connected to USB, but will have produced the "firmware.bin" file, see the command output for the file location
  * Use a browser to navigate to the ESP32 web interface (such as http://CONFIGURED_IP_FOR_ESP32/), and use the form to select the "firmware.bin" file, then click the button. New firmware version should upload and be running within the ESP32 in a matter of a few seconds


## Putting the hardware together

Zehnder Comfoair Q puts CAN BUS protocol messages in the yellow (CAN_H) and white (CAN_L) pins in the two 4-pin headers at the front of the unit, on top of the display (protected by an sliding grey shelf). Pinout being :
  * Red : DC +12 V
  * Black : DC GND
  * Yellow : CAN_H (CAN High)
  * White : CAN_L (CAN Low)

Both headers may be used in case you need to daisy-chain the MVHR unit into an existing CAN bus.

As the MVHR puts DC 12 V out between Red and Black pins, you may be tempted to feed the ESP32 development board directly of it (and feed the CAN transceiver off the ESP32), however :
  * You may still need a DC 12 V to DC 3.3 step down converter to power the CAN transceiver board
  * ESP32 dev board may accept up to 15 V, but as it uses a linear voltage regulator, than means it will dissipate relatively high amounts of power (causing it to heat up probably too much in the long term)

Physical connections will therefore be (note pin labels may vary slightly across different ESP32 and CAN transceiver boards) :
  * Power
    * MVHR Red (DC 12 V) OUT -> DC/DC converter IN+
    * MVHR Black (DC GND) OUT -> DC/DC converter IN-
    * DC/DC converter OUT+ -> ESP32 dev board 3V3 pin
    * DC/DC converter OUT+ -> CAN transceiver 3V3 pin
    * DC/DC converter OUT- -> ESP32 dev board GND pin
    * DC/DC converter OUT- -> CAN transceiver GND pin

 * Data
   * MVHR Yellow (CAN_H) OUT -> CAN transceiver CANH pin
   * MVHR White (CAN_L) OUT -> CAN transceiver CANL pin
   * CAN transceiver CAN TX (or CTX) pin -> ESP32 dev board pin 4 (GPIO4)
   * CAN transceiver CAN RX (or CRX) pin -> ESP32 dev board pin 5 (GPIO5)

Note the DC/DC converter chosen is a configurable one through the small screw on top of the blue block. It may take some effort and turning the screw back and forth a few times until being able to set a solid 3.3 V DC as output. If you happen to use a static 12 VDC to 3.3 VDC converter, you can save this pain yourself.

For the connection with the Comfoair Q 4-pin CAN header you will need some cable. Ethernet cable may do, but in this case I would recommend soldering some pins on the MVHR unit end. Using more sturdy cable such as 2 x 2 0.75 mm2 data cable available for some sources (with solid copper core) will also work.


After double checking connections for shorts and continuity, power down the Comfoair Q, and proceed to connect the wires to the CAN header on the left of the MVHR unit. There seems to be no way to get the cable routed internally out the Comfoair Q unit, and as a result the grey shelf can't be completely closed upon connecting any accesory (even if using the official Comfonnect LAN C).

Upon powering up the MVHR unit and if everything goes well you should start seeing subtopics being created and messages being sent to the MQTT broker at the configured MQTT_PREFIX.

Note some of the topics related to the fans are updated so very often that you will likely want to throttle those messages down in Home Assistant to avoid spamming your database too much with unnecessarily frequent updates. On the other hand, other topics are only published once they change state, so you may not see some of them for a while unless you manually change settings in the unit (ie changing fan speed or activating the bypass manually).


## MQTT commands to interact with the ventilation unit
This software publishes lots of values to the MQTT broker (nearly 40 in total), but it is also subscribed to the configured MQTT broker listening for the following topics being published below the `${MQTT_PREFIX}/commands/${KEY}` path, the available commands (${KEY} value) being :

  * ventilation_level_0
  * ventilation_level_1
  * ventilation_level_2
  * ventilation_level_3
  * boost_10_min
  * boost_20_min
  * boost_30_min
  * boost_60_min
  * boost_end
  * auto
  * manual
  * bypass_activate_1h
  * bypass_deactivate_1h
  * bypass_auto
  * ventilation_supply_only
  * ventilation_supply_only_reset
  * ventilation_extract_only
  * ventilation_extract_only_reset
  * ventilation_balance
  * temp_profile_normal
  * temp_profile_cool
  * temp_profile_warm

These topics (ie. "comfoair/commands/ventilation_level_3") need no payload to be acted on. There are two additional topics with take a payload as a parameter, namely :
  * ventilation_level : accepts the strings `0` or `1`, `2`, `3` as values, used to set the desired fan speed level (would be equivalent to the "ventilation_level_?" above)
  * set_mode : which accepts `auto` or `manual` as payload, and would be equivalent to the "auto" and "manual" above

You may use any visual MQTT client of your choice (ie [MQTT Explorer](http://mqtt-explorer.com/)) to see the topics and values being set, and to debug your HA config, if it does not work the first time.


## Home Assistant basic configuration

Head over to the "docs/haconfig" directory in this project and have a look into the "mqtt.yaml" file. For the Home Assistant MQTT integration to automatically have entities created for topics, they need to be published under the Home Assistant topic root in MQTT, and the "data" topics to have associated "status" topics, which this project does not provide.

The way around it is to manually define the new sensors for each of the new MQTT topics, which is what the "mqtt.yaml" file is for. Put the file in the HA configuration directory and make sure the file is loaded up from "configuration.yaml" as below :
```yaml
mqtt: !include mqtt.yaml
```

After reloading your YAML files you may use HA "Developer Tools" to search for the new sensors (a MQTT entity named "MVHR Exhaust Fan Speed" for MQTT topic "comfoair/exhaust_fan_speed" will have a sensor named "sensor.mvhr_exhaust_fan_speed" created).

File "templates.yaml" is also provided with this project with a few of the sensors having human-readable versions by using templates, so for example, you can use the sensor that reports fan speeds such as "Speed 2 (Default)" instead of just a "2". You may need to either load the "templates.yaml" file from "configuration.yaml" or add the file's contents to your existing templates config file :
```yaml
mqtt: !include templates.yaml
```

## Home Assistant advanced configuration

### Setting ventilation speed from Home Assistant GUI
Doing this requires some more configuration changes to HA, and creating a "Helper" (Settings -> Devices & Services -> Helpers) from the Home Assistant GUI to trigger MQTT commands to the broker to run the required action, namely, chosse a helperof type "Dropdown", and give it a name (ie "MVHR Ventilation Speed"), and add the four ventilation (textual) speeds as "Options" (they need to be written exactly as in "templates.yaml", if not using the template, use just the numbers 0 through 3). This will create a new entity called "input_select.mvhr_ventilation_speed"

Now add the following to the "sensor" configuration section in your HA "configuration.yaml" file, to create the associated sensors :
```yaml
  - platform: template
    sensors:
      mvhr_speed_select_state:
        value_template: "{{ states('input_select.mvhr_ventilation_speed') }}"
```

Finally, load up the "automations.yaml" file provided in this project, or extend the automations config file if you already have one, if not, add this to "configuration.yaml" :
```yaml
automation: !include automations.yaml
```

First automation listens for changes to the created "input_select", which are triggered when choosing a different ventilation speed from the GUI using the drop down. It then runs an action (defined as a template) so that depending on the value chosen in the drop down, the MQTT topic published is the corredponding one.

The second automation is to keep the selected ventilation speed in the drop down synced with the current ventilation speed (as you can still change things using the unit's physical interface and buttons). It listens to changes to the "sensor.mvhr_supply_fan_user_speed" entity, which shows the numerical value for the ventilation speed, and maps to the corresponding textual value in the drop down.

When all these bits are added in the config, YAML files reloaded, all that remains to be done is adding the "input_select.mvhr_ventilation_speed" as any other entity to a Lovelace card in the GUI.


### Setting ventilation speed from Home Assistant GUI
As in the previous case we need a "Helper" to (well, help) us both set the bypass state (activated or deactivated) and to show it in a switch-like entity in Home Assistant GUI. As we can only set the bypass to 100% for 1 hour or force disable the bypass (set it to 0%) for one hour, the right option is for an "input_boolean" (Helper of type "Toggle") to be created (gave it a name "MVHR Force Bypass" and an entity_id of "input_boolean.mvhr_bypass_on_off").

Once done create a template sensor off the helper, as per the code below to be added to "configuration.yaml" :
```yaml
  - platform: template
    sensors:
      mvhr_bypass_select_state:
        value_template: "{{ states('input_boolean.mvhr_bypass_on_off') }}"
```

So we can finally create an automation, "automations.yaml" file in this repository has been updated with both the automation so that the switch in the UI can be used to set the bypass on or off, as well as the automation so that when the bypass state is changed (forced) from the MVHR unit, the switch also changes in the UI.

Note I made the decision to consider the bypass on when open factor to be 85 % or higher, and deem the bypass closed for open factors below 85 %. Had it been possible to set the bypass open factor programmaticallly I would have chosen a different entity to show (ie a dial), but we can just set the bypass 100% open for one hour, or 0% open for one hour, besides setting it for auto, and hence some arbitrary cut-off point had to be chosen.


Note, as it is also the case for the fan speed set through MQTT, after the two hours have passed since setting the fan speed manually (or one hour for the bypass state), the MVHR unit should automatically fall back to auto mode for the given function (fan or bypass) with no need for human intervention.


## TODO

Nothing I can think so far.


