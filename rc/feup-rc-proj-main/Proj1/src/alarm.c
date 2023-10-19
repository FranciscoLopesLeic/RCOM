#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <stdbool.h>
#include "alarm.h"

static struct sigaction old_action;
static AlarmListener* alarm_listeners = NULL; 
static size_t number_of_listeners = 0;

// Alarm handler
void alarm_callback(int signo) {
    // Calls 'subhandlers' that were subscribed (behaviour like observer pattern in OP)
    for (int i = 0; i < number_of_listeners; i++) {
        alarm_listeners[i]();
    }
}

int setup_alarm_handler() {
    struct sigaction new_action;
    sigset_t smask;
    if (sigemptyset(&smask)==-1) {
        return -1;
    }

    new_action.sa_mask = smask;
    new_action.sa_handler = alarm_callback;
    new_action.sa_flags = 0;

    if (sigaction(SIGALRM, &new_action, &old_action) == -1) {
        perror("Error in alarm - setup_alarm_handler");
        return -1;
    }
    return 0;
}

// Add a new subhandler/listener to the functions to be called when the alarm 'rings'
int subscribe_alarm(AlarmListener alarm_listener) {
    number_of_listeners++;
    if (alarm_listeners == NULL) {
        if ((alarm_listeners = (AlarmListener*) malloc(sizeof(AlarmListener))) == NULL) {
            perror("Error in alarm - subscribe_alarm");
            return -1;
        }
        alarm_listeners[0] = alarm_listener;
    } else {
        AlarmListener* new_alarm_listeners;
        if ((new_alarm_listeners = realloc(alarm_listeners, sizeof(AlarmListener) * number_of_listeners)) == NULL) {
            perror("Error in alarm - subscribe_alarm");
            return -1;
        }
        alarm_listeners = new_alarm_listeners;
        alarm_listeners[number_of_listeners - 1] = alarm_listener;
    }
    return 0;
}

int restore_alarm_handler() {
    free(alarm_listeners);
    number_of_listeners = 0;
    if (sigaction(SIGALRM, &old_action, NULL) == -1) {
        return -1;
    }
    return 0;
}
