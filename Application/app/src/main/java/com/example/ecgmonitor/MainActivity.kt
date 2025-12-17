package com.example.ecgmonitor

import android.Manifest
import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothSocket
import android.content.pm.PackageManager
import android.content.res.Configuration
import android.graphics.Color
import android.os.Build
import android.os.Bundle
import android.util.Log
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import androidx.core.graphics.toColorInt
import com.example.ecgmonitor.databinding.ActivityMainBinding
import com.github.mikephil.charting.data.Entry
import com.github.mikephil.charting.data.LineData
import com.github.mikephil.charting.data.LineDataSet
import kotlinx.coroutines.*
import java.io.IOException
import java.io.InputStream
import java.util.UUID

/**
 * MainActivity - Main screen for ECG Monitor application
 * Handles Bluetooth connection to HC-05 module and displays real-time ECG data
 */
class MainActivity : AppCompatActivity() {

    /* View binding instance */
    private lateinit var binding: ActivityMainBinding

    /* Bluetooth components */
    private var bluetoothAdapter: BluetoothAdapter? = null
    private var bluetoothSocket: BluetoothSocket? = null
    private var inputStream: InputStream? = null

    /* Connection state */
    private var isConnected = false
    private var readJob: Job? = null

    /* Current BPM value - preserved during screen rotation */
    private var currentBpm = 0
    /* Standard UUID for HC-05 Bluetooth module (SPP) */
    private val MY_UUID: UUID = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB")
    private val DEVICE_NAME = "HC-05"

    /* Chart data entries - preserved during screen rotation */
    private val entries = ArrayList<Entry>()

    /* Sweep / overwrite mode */
    private companion object {
        private const val SWEEP_WINDOW_POINTS = 250   /* width of one sweep (number of points) */
        private const val Y_MIN = 0f
        private const val Y_MAX = 4095f
    }

    /* current "write head" position (0..SWEEP_WINDOW_POINTS-1) */
    private var sweepIndex = 0
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        initUI()

        val bluetoothManager = getSystemService(BLUETOOTH_SERVICE) as BluetoothManager
        bluetoothAdapter = bluetoothManager.adapter
    }

    /**
     * Handles screen orientation changes
     * Reloads UI while maintaining Bluetooth connection and chart data
     */
    @SuppressLint("SetTextI18n")
    override fun onConfigurationChanged(newConfig: Configuration) {
        super.onConfigurationChanged(newConfig)

        /* Reload UI layout without disconnecting Bluetooth */
        initUI()

        /* Restore chart data to the new chart instance */
        if (entries.isNotEmpty()) {
            val set = createSet()
            set.values = entries
            val data = LineData(set)
            data.setValueTextColor(Color.WHITE)
            binding.chart.data = data
            binding.chart.notifyDataSetChanged()

            binding.chart.setVisibleXRangeMaximum(SWEEP_WINDOW_POINTS.toFloat())
            binding.chart.moveViewToX(0f)
        }

        /* Restore connection status and BPM display */
        if (isConnected) {
            binding.btnConnect.text = "Disconnect"
            updateStatus(currentBpm)
            binding.tvBPM.text = currentBpm.toString()
        }
    }

    /**
     * Initializes the user interface
     * Shared between onCreate and onConfigurationChanged
     */
    private fun initUI() {
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        setupChart()
        resetChart()

        /* Setup connect/disconnect button listener */
        binding.btnConnect.setOnClickListener {
            if (isConnected) {
                disconnect()
            } else {
                checkPermissionsAndConnect()
            }
        }
    }

    private fun resetChart() {
        entries.clear()
        sweepIndex = 0

        val set = createSet()

        for (i in 0 until SWEEP_WINDOW_POINTS) {
            entries.add(Entry(i.toFloat(), Y_MIN))
        }
        set.values = entries

        val data = LineData(set)
        binding.chart.data = data

        binding.chart.notifyDataSetChanged()
        binding.chart.invalidate()
    }

    /**
     * Configures the ECG chart appearance and settings
     */
    private fun setupChart() {
        val chart = binding.chart
        chart.description.isEnabled = false
        chart.setTouchEnabled(false)
        chart.isDragEnabled = false
        chart.setScaleEnabled(false)
        chart.setDrawGridBackground(false)
        chart.setPinchZoom(false)
        chart.setBackgroundColor(Color.TRANSPARENT)
        chart.legend.isEnabled = false

        val set = createSet()

        entries.clear()
        for (i in 0 until SWEEP_WINDOW_POINTS) {
            entries.add(Entry(i.toFloat(), Float.NaN))
        }
        set.values = entries

        val data = LineData(set)
        chart.data = data

        chart.xAxis.isEnabled = false
        chart.axisRight.isEnabled = false

        val leftAxis = chart.axisLeft
        leftAxis.textColor = Color.WHITE
        leftAxis.setDrawGridLines(true)
        leftAxis.gridColor = "#33FFFFFF".toColorInt()
        leftAxis.axisLineColor = Color.TRANSPARENT

        /* Fix Y axis range: 0..4095 */
        leftAxis.axisMinimum = Y_MIN
        leftAxis.axisMaximum = Y_MAX

        /* Fix X window for sweep */
        chart.xAxis.axisMinimum = 0f
        chart.xAxis.axisMaximum = (SWEEP_WINDOW_POINTS - 1).toFloat()

        chart.setVisibleXRangeMaximum(SWEEP_WINDOW_POINTS.toFloat())
        chart.moveViewToX(0f)

        chart.invalidate()
    }

    /**
     * Adds a new ECG value entry to the chart
     * @param value The ECG signal value to add
     */
    private fun addEntry(value: Float) {
        val chart = binding.chart
        val data = chart.data ?: return

        var set = data.getDataSetByIndex(0) as? LineDataSet
        if (set == null) {
            set = createSet()
            data.addDataSet(set)
        }

        /* Clamp to fixed Y range 0..4095 */
        val v = value.coerceIn(Y_MIN, Y_MAX)

        /* Overwrite point at current sweep position */
        if (sweepIndex >= entries.size) sweepIndex = 0
        entries[sweepIndex].y = v

        /* Advance write head (wrap without clearing) */
        sweepIndex++
        if (sweepIndex >= SWEEP_WINDOW_POINTS) {
            sweepIndex = 0
        }

        data.notifyDataChanged()
        chart.notifyDataSetChanged()

        /* Keep the view fixed; we are overwriting in-place */
        chart.setVisibleXRangeMaximum(SWEEP_WINDOW_POINTS.toFloat())
        chart.moveViewToX(0f)
        chart.invalidate()
    }

    /**
     * Creates a new LineDataSet with ECG signal styling
     * @return Configured LineDataSet instance
     */
    private fun createSet(): LineDataSet {
        val set = LineDataSet(null, "ECG Signal")
        set.axisDependency = com.github.mikephil.charting.components.YAxis.AxisDependency.LEFT
        set.color = "#FF4081".toColorInt()
        set.lineWidth = 2f
        set.mode = LineDataSet.Mode.CUBIC_BEZIER
        set.setDrawCircles(false)
        set.setDrawValues(false)
        set.setDrawFilled(false)
        set.fillColor = "#FF4081".toColorInt()
        set.fillAlpha = 30
        set.isHighlightEnabled = false
        return set
    }

    /* Permission request launcher for Bluetooth permissions */
    private val requestPermissionLauncher =
        registerForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) { permissions ->
            val allGranted = permissions.entries.all { it.value }
            if (allGranted) {
                connectToDevice()
            } else {
                Toast.makeText(this, "Permissions denied.", Toast.LENGTH_SHORT).show()
            }
        }

    /**
     * Checks required Bluetooth permissions and initiates connection
     * Handles different permission requirements for Android 12+ and older versions
     */
    private fun checkPermissionsAndConnect() {
        val permissionsToRequest = mutableListOf<String>()

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            permissionsToRequest.add(Manifest.permission.BLUETOOTH_SCAN)
            permissionsToRequest.add(Manifest.permission.BLUETOOTH_CONNECT)
        } else {
            permissionsToRequest.add(Manifest.permission.BLUETOOTH)
            permissionsToRequest.add(Manifest.permission.BLUETOOTH_ADMIN)
            permissionsToRequest.add(Manifest.permission.ACCESS_FINE_LOCATION)
        }

        val missingPermissions = permissionsToRequest.filter {
            ContextCompat.checkSelfPermission(this, it) != PackageManager.PERMISSION_GRANTED
        }

        if (missingPermissions.isEmpty()) {
            connectToDevice()
        } else {
            requestPermissionLauncher.launch(missingPermissions.toTypedArray())
        }
    }

    /**
     * Establishes Bluetooth connection to HC-05 device
     * Runs on IO dispatcher to avoid blocking main thread
     */
    private fun connectToDevice() {
        if (ActivityCompat.checkSelfPermission(
                this,
                Manifest.permission.BLUETOOTH_CONNECT
            ) != PackageManager.PERMISSION_GRANTED && Build.VERSION.SDK_INT >= Build.VERSION_CODES.S
        ) {
            return
        }

        binding.tvStatus.text = "Scanning..."
        binding.tvStatus.setTextColor(Color.YELLOW)

        CoroutineScope(Dispatchers.IO).launch {
            try {
                val pairedDevices: Set<BluetoothDevice>? = bluetoothAdapter?.bondedDevices
                val device = pairedDevices?.find { it.name == DEVICE_NAME }

                if (device != null) {
                    withContext(Dispatchers.Main) {
                        binding.tvStatus.text = "Connecting..."
                    }
                    bluetoothAdapter?.cancelDiscovery()

                    try {
                        bluetoothSocket = device.createRfcommSocketToServiceRecord(MY_UUID)
                        bluetoothSocket?.connect()
                        inputStream = bluetoothSocket?.inputStream
                        isConnected = true

                        withContext(Dispatchers.Main) {
                            binding.tvStatus.text = "Connected"
                            binding.tvStatus.setTextColor(Color.GREEN)
                            binding.btnConnect.text = "Disconnect"
                            startReadingData()
                        }
                    } catch (e: IOException) {
                        withContext(Dispatchers.Main) {
                            binding.tvStatus.text = "Failed"
                            binding.tvStatus.setTextColor(Color.RED)
                        }
                        bluetoothSocket?.close()
                    }
                } else {
                    withContext(Dispatchers.Main) {
                        binding.tvStatus.text = "HC-05 not paired"
                        binding.tvStatus.setTextColor(Color.RED)
                    }
                }
            } catch (e: Exception) {
                withContext(Dispatchers.Main) {
                    binding.tvStatus.text = "Error"
                }
            }
        }
    }

    /**
     * Starts continuous data reading from Bluetooth input stream
     * Parses incoming data line by line and updates UI
     */
    private fun startReadingData() {
        readJob = CoroutineScope(Dispatchers.IO).launch {
            val buffer = ByteArray(1024)
            var bytes: Int
            val stringBuilder = StringBuilder()

            while (isActive && isConnected) {
                try {
                    if (inputStream != null && inputStream!!.available() > 0) {
                        bytes = inputStream!!.read(buffer)
                        val incoming = String(buffer, 0, bytes)
                        stringBuilder.append(incoming)

                        val endOfLineIndex = stringBuilder.indexOf("\n")
                        if (endOfLineIndex > 0) {
                            val dataLine = stringBuilder.substring(0, endOfLineIndex).trim()
                            stringBuilder.delete(0, endOfLineIndex + 1)

                            withContext(Dispatchers.Main) {
                                processData(dataLine)
                            }
                        }
                    }
                } catch (e: IOException) {
                    withContext(Dispatchers.Main) { disconnect() }
                    break
                }
            }
        }
    }

    /**
     * Processes incoming ECG data string
     * Expected format: "ecgValue,bpm" (e.g., "512,75")
     * @param data Raw data string from Bluetooth
     */
    private fun processData(data: String) {
        try {
            val parts = data.split(",")
            if (parts.size == 2) {
                val ecgValue = parts[0].toFloat()
                val bpm = parts[1].toInt()

                /* Store BPM for screen rotation restoration */
                currentBpm = bpm

                addEntry(ecgValue)
                binding.tvBPM.text = bpm.toString()
                updateStatus(bpm)
            }
        } catch (e: NumberFormatException) {
            Log.e("ECG", "Error parsing: $data")
        }
    }

    /**
     * Updates heart rate status based on BPM value
     * @param bpm Current beats per minute value
     */
    private fun updateStatus(bpm: Int) {
        if (!isConnected) return

        when {
            bpm < 60 -> {
                binding.tvStatus.text = "Bradycardia (Slow)"
                binding.tvStatus.setTextColor("#FFC107".toColorInt())
            }
            bpm > 100 -> {
                binding.tvStatus.text = "Tachycardia (Fast)"
                binding.tvStatus.setTextColor("#FF4081".toColorInt())
            }
            else -> {
                binding.tvStatus.text = "Normal"
                binding.tvStatus.setTextColor("#03DAC5".toColorInt())
            }
        }
    }

    /**
     * Disconnects Bluetooth connection and resets UI state
     */
    private fun disconnect() {
        try {
            readJob?.cancel()
            bluetoothSocket?.close()
            isConnected = false
            currentBpm = 0

            binding.tvStatus.text = "Disconnected"
            binding.tvStatus.setTextColor(Color.GRAY)
            binding.btnConnect.text = "Connect"
            binding.tvBPM.text = "--"

            /* Clear chart data on disconnect */
            resetChart()

        } catch (e: IOException) {
            e.printStackTrace()
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        disconnect()
    }
}