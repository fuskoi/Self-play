#include "frame_proto.h"

#include <string.h>

uint16_t frame_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

void frame_parser_reset(frame_parser_t *p)
{
    p->len = 0;
}

static bool try_parse(frame_parser_t *p, frame_t *out)
{
    if (p->len < FRAME_HEADER_SIZE + FRAME_CRC_SIZE) {
        return false;
    }
    if (p->buf[0] != FRAME_HDR0 || p->buf[1] != FRAME_HDR1) {
        return false;
    }

    uint16_t payload_len = ((uint16_t)p->buf[5] << 8) | p->buf[6];
    if (payload_len > FRAME_MAX_PAYLOAD) {
        frame_parser_reset(p);
        return false;
    }

    size_t need = FRAME_HEADER_SIZE + payload_len + FRAME_CRC_SIZE;
    if (p->len < need) {
        return false;
    }

    uint16_t got = ((uint16_t)p->buf[need - 2] << 8) | p->buf[need - 1];
    uint16_t expect = frame_crc16(&p->buf[2], FRAME_HEADER_SIZE - 2 + payload_len);
    if (got != expect) {
        /* drop one byte and resync */
        memmove(p->buf, p->buf + 1, p->len - 1);
        p->len -= 1;
        return false;
    }

    out->cmd = p->buf[2];
    out->seq = ((uint16_t)p->buf[3] << 8) | p->buf[4];
    out->len = payload_len;
    if (payload_len) {
        memcpy(out->data, &p->buf[7], payload_len);
    }

    /* keep remaining bytes after this frame */
    size_t remain = p->len - need;
    if (remain) {
        memmove(p->buf, p->buf + need, remain);
    }
    p->len = remain;
    return true;
}

bool frame_parser_feed(frame_parser_t *p, uint8_t byte, frame_t *out)
{
    if (p->len == 0) {
        if (byte != FRAME_HDR0) {
            return false;
        }
    } else if (p->len == 1) {
        if (byte != FRAME_HDR1) {
            frame_parser_reset(p);
            if (byte == FRAME_HDR0) {
                p->buf[0] = byte;
                p->len = 1;
            }
            return false;
        }
    }

    if (p->len >= sizeof(p->buf)) {
        frame_parser_reset(p);
        return false;
    }

    p->buf[p->len++] = byte;
    return try_parse(p, out);
}

size_t frame_build(uint8_t *out, size_t out_cap, uint8_t cmd, uint16_t seq,
                   const uint8_t *payload, uint16_t payload_len)
{
    size_t need = FRAME_HEADER_SIZE + payload_len + FRAME_CRC_SIZE;
    if (out_cap < need || payload_len > FRAME_MAX_PAYLOAD) {
        return 0;
    }

    out[0] = FRAME_HDR0;
    out[1] = FRAME_HDR1;
    out[2] = cmd;
    out[3] = (uint8_t)(seq >> 8);
    out[4] = (uint8_t)(seq & 0xFF);
    out[5] = (uint8_t)(payload_len >> 8);
    out[6] = (uint8_t)(payload_len & 0xFF);
    if (payload_len && payload) {
        memcpy(&out[7], payload, payload_len);
    }

    uint16_t crc = frame_crc16(&out[2], FRAME_HEADER_SIZE - 2 + payload_len);
    out[7 + payload_len] = (uint8_t)(crc >> 8);
    out[8 + payload_len] = (uint8_t)(crc & 0xFF);
    return need;
}
