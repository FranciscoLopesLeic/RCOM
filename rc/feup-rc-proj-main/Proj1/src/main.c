#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <getopt.h>
#include <time.h>
#include "error.h"
#include "state_machine.h"
#include "application.h"

void print_cmd_args() {
    printf("Usage: \n");
    printf("    rcompy -r/-t --file <argument> --serial_port <argument> [--fer <argument> --t_prop <argument> --help]\n");
    printf("Options: \n");
    printf("    -r               Receiver mode\n");
    printf("    -t               Transmitter mode\n");
    printf("    -f --file        Choose destination if receiver or source if transmitter\n");
    printf("    -s --serial_port Choose serial port path\n");
    printf("    -z --header_error_ratio        Choose error generation probability in information packets\n");
    printf("    -x --data_error_ratio         Choose error generation probability in information packets\n");
    printf("    -p --t_prop      Choose propagation delay in milliseconds\n");
    printf("    -h --help        Show this help message\n");
}

// Just parsing
int main(int argc, char** argv) {
    
    srandom(time(NULL));
    struct option options[] = {
        {"file", required_argument, 0, 'f'},
        {"serial_port", required_argument, 0, 's'},
        {"header_error_ratio", required_argument, 0, 'z'},
        {"data_error_ratio", required_argument, 0, 'x'},
        {"t_prop", required_argument, 0, 'p'},
        {"help", required_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    bool is_trans, is_rec, had_file_path, had_serial_path, had_header_error, had_data_error, had_tprop;
    is_trans = is_rec = had_file_path = had_serial_path = had_header_error = had_data_error = had_tprop = false;

    char* file_path;
    char* serial_port_path;
    int t_prop = 0;
    double header_error = 0;
    double data_error = 0;

    int c = 0;
    int opt_index = 0;
    while((c = getopt_long(argc, argv, "htrf:p:x:z:s:", options, &opt_index)) != -1) {
        switch (c) {
            case 't': {
                is_trans = true;
                if (is_rec) {
                    print_cmd_args();
                    return 1;
                }
                break;
            }
            case 'r': {
                is_rec = true;
                if (is_trans) {
                    print_cmd_args();
                    return 1;
                }
                break;
            }
            case 'f': {
                if (had_file_path) {
                    print_cmd_args();
                    return 1;
                }
                had_file_path = true;
                file_path = optarg;
                break;
            }
            case 's': {
                if (had_serial_path) {
                    print_cmd_args();
                    return 1;
                }
                had_serial_path = true;
                serial_port_path = optarg;
                break;
            }
            case 'p': {
                if (had_tprop) {
                    print_cmd_args();
                    return 1;
                }
                had_tprop = true;
                t_prop = atoi(optarg);
                break;
            }
            case 'z': {
                if (had_header_error) {
                    print_cmd_args();
                    return 1;
                }
                had_header_error = true;
                header_error = strtod(optarg, NULL);
                break;
            }
            case 'x': {
                if (had_data_error) {
                    print_cmd_args();
                    return 1;
                }
                had_data_error = true;
                data_error = strtod(optarg, NULL);
                break;
            }
            case 'h': {
                print_cmd_args();
                return 1;
            }
            default: {
                print_cmd_args();
                return 1;
            }
        }
    }
    if (!had_file_path || !had_serial_path || (!is_rec && !is_trans)) {
        print_cmd_args();
        return 1;
    }

    set_error_rates(header_error, data_error);
    set_prop_time(t_prop);
    init_app(is_rec ? RECEIVER : TRANSMITTER, file_path, serial_port_path);

    return 0;
}