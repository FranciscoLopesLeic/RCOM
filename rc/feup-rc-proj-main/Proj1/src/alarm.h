#pragma once

typedef void (*AlarmListener)(); 

int setup_alarm_handler();
int subscribe_alarm(AlarmListener alarm_listener);
int restore_alarm_handler();
