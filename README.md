# rikarduino

ESP32 based temperature control for RIKA Visio pellet stove.

Goal was to replace and improve the uncomfortable LCD thermostat with -2 / +2 Celsius ON/OFF hysteresis, or the 7-seg not-thermostat with manual control 0~100% duty cycle.

This sits as a man-in-the-middle in the communication line between the central unit and the 7-segment display. 

Features:
- It can decode any button press, and what is displayed on the 7-seg: menus, statuses, errors, etc.
- It can then change the duty cycle 0~100% according to the current and target room temperature.
- Uses PID for most stable temperature. This needs to be tuned to your own room size and configuration, which is difficult due the the extreme lag between commands and response, as well as the self cleaning adding more heat when not necessarily wanted.
- Can switch the oven On/Off remotely, or based on target temperature, or based on a schedule
- Schedule calendar with different target temperatures based on time
- PHP project for the web backend/frontend and remote monitoring/control (could work without)
- mongoDB for graph view and history (could work without)
- Works standalone over WIFI. No need for LCD thermostat, nor GSM module, nor Pellet Control.
- This has integrated temperature sensor but it would be better to have the temp sensor further away or outside of the electronics, due to the ESP32 and OLED displays heating in the plastic case.
- Can detect low pellet shortly before it's empty (when temp doesn't increase enough) and send an email accordingly.
- optional relay to revert to the original LCD thermostat (not implemented) at any time.
