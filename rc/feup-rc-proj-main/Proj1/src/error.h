#pragma once

#include <stdbool.h>
#include <stdint.h>


void delay();
void set_error_rates(double h_error, double d_error);
void set_prop_time(int t_prop);
bool should_corrupt_data();
bool should_corrupt_header();
void corrupt_header(uint8_t* a, uint8_t* c, uint8_t* bcc1);
void corrupt_data_buffer(uint8_t* data_buffer, size_t buffer_size);