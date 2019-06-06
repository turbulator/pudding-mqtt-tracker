# Pudding MQTT Tracker
GPS/GPRS tracker software for A9G module

This project will be useful for tracking location of vehicles (or another moving objects).
I'm using tracker with JSON MQTT Device Tracker module of my Home Assistant.

# The concept
I connected the power supply of A9G to IGN wire of my car. So, tracker will automatically
connect to my MQTT broker during next 10-20 seconds after switch on IGN. State led will blink while connecting.

After start and GPRS activation tracker publishes state message (%IMEI% will be
changed by actual IMEI number):

```
topic: location/%IMEI%/state
payload: online
```

Further tracker publishes coordinates in json format every few seconds. Data led will blink.

```
topic: location/%IMEI%
payload: {"longitude": 0.000,"latitude": 0.000}
```

Tracker sets will message to notify subscribers when it goes offline.  

```
topic: location/%IMEI%/state
payload: offline
```

# Preparing
You should setup toolchain and SDK. Please have a look on [AI-Thinker's page](https://ai-thinker-open.github.io/GPRS_C_SDK_DOC/en/c-sdk/installation_linux.html)

Further change app folder of GPRS_C_SDK by mine

# Making firmware

Run build script as usual:
```
[user@host GPRS_C_SDK]$ ./build.sh app release

===================Start build=====================
-- Param   number : 1
-- Compile mode   : release
-- Core    number : 4
-- Compile project: app
-- Project path   :
  |
   --/home/user/Projects/GPRS_C_SDK/app
---------------------------------------------------
-- System Version : LINUX
---------------------------------------------------


[MAKE]  init

[CC]   sdk_init.o                  <== sdk_init.c
[AR]   libinit_release.a
...
```
# Flashing firmware

Please have a look on [AI-Thinker's page](https://ai-thinker-open.github.io/GPRS_C_SDK_DOC/en/c-sdk/burn-debug.html)

# Customizing

You should place parameters of your own MQTT broker in the head of app_main.c
