#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define FRAME_HDR0           0xAA
#define FRAME_HDR1           0x55
#define FRAME_MAX_PAYLOAD    1024
#define FRAME_HEADER_SIZE    7
#define FRAME_CRC_SIZE       2
#define FRAME_MAX_SIZE       (FRAME_HEADER_SIZE + FRAME_MAX_PAYLOAD + FRAME_CRC_SIZE)

#define CMD_DATA             0x01
#define CMD_END              0x02
#define CMD_ABORT            0x03
#define CMD_ACK              0x81
#define CMD_NAK              0x82

#define ERR_CRC              1
#define ERR_SEQ              2
#define ERR_OVERFLOW         3
#define ERR_BUSY             4
#define ERR_BAD_CMD          5

typedef struct {
    uint8_t cmd;
    uint16_t seq;
    uint16_t len;
    uint8_t data[FRAME_MAX_PAYLOAD];
} frame_t;

typedef struct {
    uint8_t buf[FRAME_MAX_SIZE];
    size_t len;
} frame_parser_t;

void frame_parser_reset(frame_parser_t *p);
bool frame_parser_feed(frame_parser_t *p, uint8_t byte, frame_t *out);
uint16_t frame_crc16(const uint8_t *data, size_t len);
size_t frame_build(uint8_t *out, size_t out_cap, uint8_t cmd, uint16_t seq,
                   const uint8_t *payload, uint16_t payload_len);
