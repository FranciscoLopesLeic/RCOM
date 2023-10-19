#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "error.h"
#include "macros.h"


static double header_error_rate = 0;
static double data_error_rate = 0;
static int prop_time = 0;


bool should_corrupt(double error_rate);
uint8_t gen_corrupted_byte();




bool should_corrupt(double error_rate) {
    double prob = (double) random()/ (double) RAND_MAX;
    return (prob <= error_rate);
}

bool should_corrupt_data() {
    return should_corrupt(data_error_rate);
}

bool should_corrupt_header() {
    return should_corrupt(header_error_rate);
}

void corrupt_header(uint8_t* a, uint8_t* c, uint8_t* bcc1) {
    printf("Generated header error\n");
    uint64_t corrupted_byte = random() % 3;
    switch (corrupted_byte) {
        case 0:
            *a = gen_corrupted_byte();
            break;
        case 1:
            *c = gen_corrupted_byte();
            break;
        case 2:
            *bcc1 = gen_corrupted_byte();
            break;
        default:
            fprintf(stderr, "Error in error.c - corrupt_header: unexpected corrupted_byte\n");
            break;
    }
}

void corrupt_data_buffer(uint8_t* data_buffer, size_t buffer_size) {
    printf("Generated data error\n");
    size_t corrupted_byte_idx = random() % buffer_size;
    data_buffer[corrupted_byte_idx] = gen_corrupted_byte();
}

uint8_t gen_corrupted_byte() {
    return ((uint8_t) random());
}

void set_error_rates(double h_error, double d_error) {
    header_error_rate = h_error;
    data_error_rate = d_error;
}

void set_prop_time(int t_prop) {
    prop_time = t_prop;
}

void delay() {
    usleep(1000*prop_time);
}
