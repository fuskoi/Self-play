package com.esp.texthid

object FrameProto {
    const val HDR0: Byte = 0xAA.toByte()
    const val HDR1: Byte = 0x55
    const val HEADER_SIZE = 7
    const val CRC_SIZE = 2
    const val MAX_PAYLOAD = 1024

    const val CMD_DATA: Byte = 0x01
    const val CMD_END: Byte = 0x02
    const val CMD_ABORT: Byte = 0x03
    const val CMD_ACK: Byte = 0x81.toByte()
    const val CMD_NAK: Byte = 0x82.toByte()

    data class Frame(
        val cmd: Byte,
        val seq: Int,
        val payload: ByteArray
    )

    fun crc16(data: ByteArray, offset: Int = 0, length: Int = data.size - offset): Int {
        var crc = 0xFFFF
        for (i in offset until offset + length) {
            crc = crc xor (data[i].toInt() and 0xFF)
            repeat(8) {
                crc = if ((crc and 1) != 0) {
                    (crc ushr 1) xor 0xA001
                } else {
                    crc ushr 1
                }
            }
        }
        return crc and 0xFFFF
    }

    fun build(cmd: Byte, seq: Int, payload: ByteArray = ByteArray(0)): ByteArray {
        require(payload.size <= MAX_PAYLOAD)
        val out = ByteArray(HEADER_SIZE + payload.size + CRC_SIZE)
        out[0] = HDR0
        out[1] = HDR1
        out[2] = cmd
        out[3] = ((seq ushr 8) and 0xFF).toByte()
        out[4] = (seq and 0xFF).toByte()
        out[5] = ((payload.size ushr 8) and 0xFF).toByte()
        out[6] = (payload.size and 0xFF).toByte()
        if (payload.isNotEmpty()) {
            System.arraycopy(payload, 0, out, 7, payload.size)
        }
        val crc = crc16(out, 2, HEADER_SIZE - 2 + payload.size)
        out[7 + payload.size] = ((crc ushr 8) and 0xFF).toByte()
        out[8 + payload.size] = (crc and 0xFF).toByte()
        return out
    }

    class Parser {
        private val buf = ByteArray(HEADER_SIZE + MAX_PAYLOAD + CRC_SIZE)
        private var len = 0

        fun reset() {
            len = 0
        }

        fun feed(byte: Byte): Frame? {
            if (len == 0) {
                if (byte != HDR0) return null
            } else if (len == 1) {
                if (byte != HDR1) {
                    reset()
                    if (byte == HDR0) {
                        buf[0] = byte
                        len = 1
                    }
                    return null
                }
            }

            if (len >= buf.size) {
                reset()
                return null
            }
            buf[len++] = byte
            return tryParse()
        }

        private fun tryParse(): Frame? {
            if (len < HEADER_SIZE + CRC_SIZE) return null
            if (buf[0] != HDR0 || buf[1] != HDR1) return null

            val payloadLen = ((buf[5].toInt() and 0xFF) shl 8) or (buf[6].toInt() and 0xFF)
            if (payloadLen > MAX_PAYLOAD) {
                reset()
                return null
            }

            val need = HEADER_SIZE + payloadLen + CRC_SIZE
            if (len < need) return null

            val got = ((buf[need - 2].toInt() and 0xFF) shl 8) or (buf[need - 1].toInt() and 0xFF)
            val expect = crc16(buf, 2, HEADER_SIZE - 2 + payloadLen)
            if (got != expect) {
                System.arraycopy(buf, 1, buf, 0, len - 1)
                len -= 1
                return null
            }

            val payload = if (payloadLen > 0) {
                buf.copyOfRange(7, 7 + payloadLen)
            } else {
                ByteArray(0)
            }
            val frame = Frame(
                cmd = buf[2],
                seq = ((buf[3].toInt() and 0xFF) shl 8) or (buf[4].toInt() and 0xFF),
                payload = payload
            )

            val remain = len - need
            if (remain > 0) {
                System.arraycopy(buf, need, buf, 0, remain)
            }
            len = remain
            return frame
        }
    }
}
