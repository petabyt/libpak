#pragma once
#include <stdint.h>

struct __attribute__((packed)) Request {
	uint8_t magic;
	uint16_t transaction_counter;
	uint16_t command;
	uint64_t payload_length;
	uint8_t message_counter;
	uint8_t payload[];
};

struct __attribute__((packed)) Response {
	uint8_t magic;
	uint16_t control;
	uint16_t command;
	uint64_t payload_length;
	uint8_t message_counter;
	uint8_t payload[];
};
