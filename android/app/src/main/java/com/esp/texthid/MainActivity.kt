package com.esp.texthid

import android.Manifest
import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothManager
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.widget.ArrayAdapter
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import androidx.lifecycle.lifecycleScope
import com.esp.texthid.databinding.ActivityMainBinding
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

class MainActivity : AppCompatActivity() {
    private lateinit var binding: ActivityMainBinding
    private val client = SppClient()
    private val devices = mutableListOf<BluetoothDevice>()
    private lateinit var deviceAdapter: ArrayAdapter<String>

    private val permissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { result ->
        if (result.values.all { it }) {
            refreshDevices()
        } else {
            setStatus("需要蓝牙权限才能连接 ESP32")
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        deviceAdapter = ArrayAdapter(this, android.R.layout.simple_spinner_dropdown_item, mutableListOf())
        binding.deviceSpinner.adapter = deviceAdapter

        binding.btnRefresh.setOnClickListener { ensurePermissionsThen { refreshDevices() } }
        binding.btnConnect.setOnClickListener { toggleConnect() }
        binding.btnSend.setOnClickListener { sendText() }
        binding.btnAbort.setOnClickListener { abortSend() }

        ensurePermissionsThen { refreshDevices() }
    }

    override fun onDestroy() {
        client.disconnect()
        super.onDestroy()
    }

    private fun neededPermissions(): Array<String> {
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            arrayOf(
                Manifest.permission.BLUETOOTH_CONNECT,
                Manifest.permission.BLUETOOTH_SCAN
            )
        } else {
            emptyArray()
        }
    }

    private fun hasPermissions(): Boolean {
        return neededPermissions().all {
            ContextCompat.checkSelfPermission(this, it) == PackageManager.PERMISSION_GRANTED
        }
    }

    private fun ensurePermissionsThen(block: () -> Unit) {
        val need = neededPermissions().filter {
            ContextCompat.checkSelfPermission(this, it) != PackageManager.PERMISSION_GRANTED
        }
        if (need.isEmpty()) {
            block()
        } else {
            permissionLauncher.launch(need.toTypedArray())
        }
    }

    private fun adapter(): BluetoothAdapter? {
        val mgr = getSystemService(BluetoothManager::class.java)
        return mgr?.adapter
    }

    @SuppressLint("MissingPermission")
    private fun refreshDevices() {
        if (!hasPermissions()) return
        val bt = adapter()
        if (bt == null) {
            setStatus("本机没有蓝牙")
            return
        }
        if (!bt.isEnabled) {
            setStatus("请先打开手机蓝牙")
            return
        }

        devices.clear()
        val labels = mutableListOf<String>()
        val bonded = bt.bondedDevices.orEmpty()
            .sortedBy { it.name ?: it.address }
        for (d in bonded) {
            devices.add(d)
            val name = d.name ?: "未知设备"
            labels.add("$name  (${d.address})")
        }
        deviceAdapter.clear()
        deviceAdapter.addAll(labels)
        deviceAdapter.notifyDataSetChanged()

        if (devices.isEmpty()) {
            setStatus("没有已配对设备。请先在系统设置里配对 ESP-TEXT-KB（PIN 1234）")
        } else {
            val preferred = devices.indexOfFirst {
                (it.name ?: "").contains("ESP-TEXT-KB", ignoreCase = true)
            }
            if (preferred >= 0) {
                binding.deviceSpinner.setSelection(preferred)
            }
            setStatus("已找到 ${devices.size} 个已配对设备，选中 ESP-TEXT-KB 后点连接")
        }
    }

    @SuppressLint("MissingPermission")
    private fun toggleConnect() {
        if (client.connected) {
            client.disconnect()
            updateConnectedUi(false)
            setStatus("已断开")
            return
        }

        ensurePermissionsThen {
            val idx = binding.deviceSpinner.selectedItemPosition
            if (idx < 0 || idx >= devices.size) {
                Toast.makeText(this, "请先选择设备", Toast.LENGTH_SHORT).show()
                return@ensurePermissionsThen
            }
            val device = devices[idx]
            setStatus("正在连接 ${device.name ?: device.address} …")
            binding.btnConnect.isEnabled = false
            lifecycleScope.launch {
                try {
                    withContext(Dispatchers.IO) { client.connect(device) }
                    updateConnectedUi(true)
                    setStatus("已连接 ${device.name ?: device.address}")
                } catch (e: Exception) {
                    client.disconnect()
                    updateConnectedUi(false)
                    setStatus("连接失败: ${e.message}")
                } finally {
                    binding.btnConnect.isEnabled = true
                }
            }
        }
    }

    private fun sendText() {
        val text = binding.input.text?.toString().orEmpty()
        if (text.isBlank()) {
            Toast.makeText(this, "请输入文字", Toast.LENGTH_SHORT).show()
            return
        }
        if (!client.connected) {
            Toast.makeText(this, "请先连接", Toast.LENGTH_SHORT).show()
            return
        }

        binding.btnSend.isEnabled = false
        binding.btnAbort.isEnabled = true
        setStatus("发送中…")
        lifecycleScope.launch {
            try {
                client.sendText(text) { sent, total ->
                    runOnUiThread {
                        setStatus("已发送 $sent / $total 字节")
                    }
                }
                setStatus("发送完成，单片机正在向电脑输入")
            } catch (e: Exception) {
                setStatus("发送失败: ${e.message}")
            } finally {
                binding.btnSend.isEnabled = client.connected
                binding.btnAbort.isEnabled = client.connected
            }
        }
    }

    private fun abortSend() {
        lifecycleScope.launch {
            try {
                client.abort()
                setStatus("已发送中止")
            } catch (e: Exception) {
                setStatus("中止失败: ${e.message}")
            }
        }
    }

    private fun updateConnectedUi(connected: Boolean) {
        binding.btnConnect.text = if (connected) getString(R.string.disconnect) else getString(R.string.connect)
        binding.btnSend.isEnabled = connected
        binding.btnAbort.isEnabled = connected
        binding.deviceSpinner.isEnabled = !connected
        binding.btnRefresh.isEnabled = !connected
    }

    private fun setStatus(msg: String) {
        binding.status.text = msg
    }
}
