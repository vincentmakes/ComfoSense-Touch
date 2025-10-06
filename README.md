# Zehnder Comfoair Q350 MQTT bridge + Touch Screen

This software is inspired by the work of many others who successfully managed to replace the hardware bridge from Zehnder called "Comfoconnect LAN C" by an ESP32 + CAN transceiver. This new device is meant to not only replace the ComfoConnect but also the ComfoSense controller display which is the default display typically installed in the house to interact with the ComfoAir.

<img width="150" alt="image" src="https://github.com/user-attachments/assets/686ae7ac-9415-4ca7-90f9-afdd3ad098ec" /> ---> <img width="300"  alt="PoC_UpgradeMVHRController" src="https://github.com/user-attachments/assets/7c103ed0-92e9-4e24-a6c7-e6dc57efff5c" />




This version addresses few other issues and has the following features:
1. Wifi connection close to the Comfoair can be limited due to its location (typically the attic or the cellar), hence bringing the IoT device closer to a central area in the house (where the ComfoSense display controller normally sits) mitigate this.
2. Better user interface than the one ComfoSense, with all basic functions exposed in one screen
3. Provides exact number of days before filter change is needed (instead of the generic message Expect change of filter soon ... for 3 weeks)
4. Provides additional sensor data coming from the MVHR (temperature, humidity)
5. Provide integration with HomeAssistant via MQTT, similarly to the original ESP32 + CAN Transceiver program

This means this display can be used also by people who are not interested in the HA integration but simply want a better UI/UX than the one provided by Zehnder

## Hardware Components

Prerequisites:

* Specifically the Waveshare ESP32S3 4 inch Touch display Dev Board (contains an embedded CAN transceiver)
* A 230V->12V DC mini power supply : this is needed as the 12V from ComfoAir supplies a max of 40mA which is not enough to power the screen + ESP32

### Very Important
* Do not connect the 12V of the power supply to the one of the MVHR nor to the Waveshare ESP32
* Connect the Ground of the power supply to the one of the MVHR.
### Wiring
<img width="400" alt="image" src="https://github.com/user-attachments/assets/901908b0-c0ff-4c94-a58b-7c68779ed00d" />


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


