package com.esp.texthid

import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothSocket
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.coroutines.withContext
import java.io.IOException
import java.io.InputStream
import java.io.OutputStream
import java.util.UUID
import java.util.concurrent.LinkedBlockingQueue
import java.util.concurrent.TimeUnit

@SuppressLint("MissingPermission")
class SppClient {
    companion object {
        private val SPP_UUID: UUID = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB")
        private const val ACK_TIMEOUT_MS = 5_000L
        private const val CHUNK = FrameProto.MAX_PAYLOAD
    }

    private var socket: BluetoothSocket? = null
    private var input: InputStream? = null
    private var output: OutputStream? = null
    private val parser = FrameProto.Parser()
    private val incoming = LinkedBlockingQueue<FrameProto.Frame>()
    private val parserLock = Any()
    private var readerThread: Thread? = null
    private val writeMutex = Mutex()

    @Volatile
    var connected: Boolean = false
        private set

    fun connect(device: BluetoothDevice) {
        disconnect()
        val sock = device.createRfcommSocketToServiceRecord(SPP_UUID)
        BluetoothAdapter.getDefaultAdapter()?.cancelDiscovery()
        sock.connect()
        socket = sock
        input = sock.inputStream
        output = sock.outputStream
        synchronized(parserLock) {
            parser.reset()
            incoming.clear()
        }
        connected = true
        startReader()
    }

    fun disconnect() {
        connected = false
        readerThread?.interrupt()
        readerThread = null
        try {
            input?.close()
        } catch (_: Exception) {
        }
        try {
            output?.close()
        } catch (_: Exception) {
        }
        try {
            socket?.close()
        } catch (_: Exception) {
        }
        input = null
        output = null
        socket = null
        synchronized(parserLock) {
            incoming.clear()
            parser.reset()
        }
    }

    private fun startReader() {
        val inStream = input ?: return
        readerThread = Thread({
            val buf = ByteArray(1024)
            try {
                while (connected && !Thread.currentThread().isInterrupted) {
                    val n = inStream.read(buf)
                    if (n < 0) break
                    for (i in 0 until n) {
                        val frame = synchronized(parserLock) { parser.feed(buf[i]) }
                        if (frame != null) {
                            incoming.offer(frame)
                        }
                    }
                }
            } catch (_: Exception) {
            } finally {
                connected = false
            }
        }, "spp-reader").also { it.isDaemon = true; it.start() }
    }

    private fun writeRaw(bytes: ByteArray) {
        val out = output ?: throw IOException("not connected")
        out.write(bytes)
        out.flush()
    }

    private fun waitAck(expectSeq: Int): FrameProto.Frame {
        val deadline = System.nanoTime() + TimeUnit.MILLISECONDS.toNanos(ACK_TIMEOUT_MS)
        while (true) {
            val remainMs = TimeUnit.NANOSECONDS.toMillis(deadline - System.nanoTime())
            if (remainMs <= 0) throw IOException("ACK timeout seq=$expectSeq")
            val frame = incoming.poll(remainMs, TimeUnit.MILLISECONDS)
                ?: throw IOException("ACK timeout seq=$expectSeq")
            when (frame.cmd) {
                FrameProto.CMD_ACK -> {
                    if (frame.seq == expectSeq) return frame
                }
                FrameProto.CMD_NAK -> {
                    val err = if (frame.payload.isNotEmpty()) frame.payload[0].toInt() and 0xFF else -1
                    throw IOException("NAK seq=${frame.seq} err=$err")
                }
            }
        }
    }

    suspend fun sendText(text: String, onProgress: (Int, Int) -> Unit = { _, _ -> }) =
        withContext(Dispatchers.IO) {
            writeMutex.withLock {
                if (!connected) throw IOException("not connected")
                val bytes = text.toByteArray(Charsets.UTF_8)
                if (bytes.isEmpty()) throw IOException("empty text")
                if (bytes.size > 64 * 1024) throw IOException("text too large (>64KB)")

                synchronized(parserLock) {
                    incoming.clear()
                    parser.reset()
                }

                var seq = 0
                var offset = 0
                while (offset < bytes.size) {
                    val end = minOf(offset + CHUNK, bytes.size)
                    val chunk = bytes.copyOfRange(offset, end)
                    writeRaw(FrameProto.build(FrameProto.CMD_DATA, seq, chunk))
                    waitAck(seq)
                    offset = end
                    seq++
                    onProgress(offset, bytes.size)
                }

                writeRaw(FrameProto.build(FrameProto.CMD_END, seq))
                waitAck(seq)
            }
        }

    suspend fun abort() = withContext(Dispatchers.IO) {
        writeMutex.withLock {
            if (!connected) return@withLock
            writeRaw(FrameProto.build(FrameProto.CMD_ABORT, 0))
            incoming.poll(500, TimeUnit.MILLISECONDS)
            synchronized(parserLock) {
                incoming.clear()
                parser.reset()
            }
        }
    }
}
