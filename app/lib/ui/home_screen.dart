import 'dart:async';

import 'package:flutter/material.dart';
import 'package:permission_handler/permission_handler.dart';

import '../ble/ble_constants.dart';
import '../ble/ble_service.dart';
import '../logging/recording_service.dart';
import 'sessions_screen.dart';

class HomeScreen extends StatefulWidget {
  const HomeScreen({super.key});

  @override
  State<HomeScreen> createState() => _HomeScreenState();
}

class _HomeScreenState extends State<HomeScreen> {
  final BleService _ble = BleService();
  final RecordingService _recorder = RecordingService.instance;

  BleConnectionStatus _status = BleConnectionStatus.disconnected;
  int _soundLevel = 0;
  double _threshold = 70;
  bool _ledEnabled = true;
  bool _vibEnabled = true;

  StreamSubscription? _statusSub;
  StreamSubscription? _levelSub;

  @override
  void initState() {
    super.initState();
    _requestPermissions();

    _statusSub = _ble.statusStream.listen((status) {
      setState(() => _status = status);
      if (status == BleConnectionStatus.connected) {
        _readConfig();
      }
    });

    _levelSub = _ble.soundLevelStream.listen((level) {
      setState(() => _soundLevel = level);
    });
  }

  Future<void> _requestPermissions() async {
    await [
      Permission.bluetoothScan,
      Permission.bluetoothConnect,
      Permission.locationWhenInUse,
      Permission.notification,
    ].request();
  }

  Future<void> _readConfig() async {
    await Future.delayed(const Duration(milliseconds: 200));
    if (_status != BleConnectionStatus.connected) return;

    final threshold = await _ble.readThreshold();
    final mode = await _ble.readFeedbackMode();
    setState(() {
      _threshold = threshold.toDouble();
      _ledEnabled = (mode & feedbackModeLed) != 0;
      _vibEnabled = (mode & feedbackModeVibration) != 0;
    });
  }

  int _buildFeedbackBitmask() {
    int mask = 0;
    if (_ledEnabled) mask |= feedbackModeLed;
    if (_vibEnabled) mask |= feedbackModeVibration;
    return mask;
  }

  Future<void> _toggleRecording() async {
    if (_recorder.isRecording.value) {
      await _recorder.stopRecording();
    } else {
      await _recorder.startRecording(_ble);
    }
  }

  @override
  void dispose() {
    _statusSub?.cancel();
    _levelSub?.cancel();
    _ble.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('InsideVoice Debug'),
        actions: [
          IconButton(
            icon: const Icon(Icons.folder_open),
            tooltip: 'Recordings',
            onPressed: () => Navigator.push(
              context,
              MaterialPageRoute(builder: (_) => const SessionsScreen()),
            ),
          ),
        ],
      ),
      body: SingleChildScrollView(
        padding: const EdgeInsets.all(16),
        child: Column(
          children: [
            _connectionCard(),
            const SizedBox(height: 12),
            _soundLevelCard(),
            const SizedBox(height: 12),
            _recordingCard(),
            const SizedBox(height: 12),
            _thresholdCard(),
            const SizedBox(height: 12),
            _feedbackModeCard(),
          ],
        ),
      ),
    );
  }

  Widget _connectionCard() {
    final String label;
    final Color color;
    switch (_status) {
      case BleConnectionStatus.disconnected:
        label = 'Disconnected';
        color = Colors.red;
      case BleConnectionStatus.scanning:
        label = 'Scanning...';
        color = Colors.orange;
      case BleConnectionStatus.connecting:
        label = 'Connecting...';
        color = Colors.orange;
      case BleConnectionStatus.connected:
        label = 'Connected';
        color = Colors.green;
    }

    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Row(
              children: [
                const Text('Connection',
                    style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold)),
                const Spacer(),
                Chip(
                  label: Text(label),
                  backgroundColor: color.withValues(alpha: 0.2),
                  labelStyle: TextStyle(color: color),
                ),
              ],
            ),
            const SizedBox(height: 8),
            _status == BleConnectionStatus.connected
                ? ElevatedButton(
                    onPressed: _ble.disconnect,
                    child: const Text('Disconnect'),
                  )
                : ElevatedButton(
                    onPressed: _status == BleConnectionStatus.disconnected
                        ? _ble.scan
                        : null,
                    child: const Text('Scan & Connect'),
                  ),
          ],
        ),
      ),
    );
  }

  Widget _soundLevelCard() {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            const Text('Sound Level',
                style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold)),
            const SizedBox(height: 12),
            Center(
              child: Text(
                '$_soundLevel dB',
                style: const TextStyle(
                    fontSize: 48, fontWeight: FontWeight.w300),
              ),
            ),
            const SizedBox(height: 8),
            LinearProgressIndicator(
              value: (_soundLevel / 120).clamp(0.0, 1.0),
              minHeight: 8,
              backgroundColor: Colors.grey.shade800,
              color: _soundLevel >= _threshold ? Colors.red : Colors.green,
            ),
            const SizedBox(height: 4),
            Text(
              'Threshold: ${_threshold.round()} dB',
              style: TextStyle(color: Colors.grey.shade400, fontSize: 12),
              textAlign: TextAlign.end,
            ),
          ],
        ),
      ),
    );
  }

  Widget _recordingCard() {
    return ValueListenableBuilder<bool>(
      valueListenable: _recorder.isRecording,
      builder: (context, recording, _) {
        return ValueListenableBuilder<Duration>(
          valueListenable: _recorder.elapsed,
          builder: (context, elapsed, _) {
            final minutes = elapsed.inMinutes.remainder(60).toString().padLeft(2, '0');
            final seconds = elapsed.inSeconds.remainder(60).toString().padLeft(2, '0');
            final hours = elapsed.inHours;
            final timeStr = hours > 0
                ? '$hours:$minutes:$seconds'
                : '$minutes:$seconds';

            return Card(
              child: Padding(
                padding: const EdgeInsets.all(16),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.stretch,
                  children: [
                    Row(
                      children: [
                        const Text('Recording',
                            style: TextStyle(
                                fontSize: 18, fontWeight: FontWeight.bold)),
                        const Spacer(),
                        if (recording)
                          Chip(
                            label: Text(timeStr),
                            backgroundColor:
                                Colors.red.withValues(alpha: 0.2),
                            labelStyle: const TextStyle(color: Colors.red),
                          ),
                      ],
                    ),
                    const SizedBox(height: 8),
                    ElevatedButton.icon(
                      onPressed: _toggleRecording,
                      icon: Icon(recording ? Icons.stop : Icons.fiber_manual_record),
                      label: Text(recording ? 'Stop Recording' : 'Start Recording'),
                      style: recording
                          ? ElevatedButton.styleFrom(
                              backgroundColor: Colors.red.withValues(alpha: 0.2),
                            )
                          : null,
                    ),
                  ],
                ),
              ),
            );
          },
        );
      },
    );
  }

  Widget _thresholdCard() {
    final connected = _status == BleConnectionStatus.connected;

    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            const Text('Threshold',
                style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold)),
            const SizedBox(height: 8),
            Row(
              children: [
                const Text('50'),
                Expanded(
                  child: Slider(
                    value: _threshold,
                    min: 50,
                    max: 90,
                    divisions: 40,
                    label: '${_threshold.round()} dB',
                    onChanged: connected
                        ? (v) => setState(() => _threshold = v)
                        : null,
                    onChangeEnd: connected
                        ? (v) => _ble.writeThreshold(v.round())
                        : null,
                  ),
                ),
                const Text('90'),
              ],
            ),
            Text(
              '${_threshold.round()} dB',
              textAlign: TextAlign.center,
              style: const TextStyle(fontSize: 16),
            ),
          ],
        ),
      ),
    );
  }

  Widget _feedbackModeCard() {
    final connected = _status == BleConnectionStatus.connected;

    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            const Text('Feedback Mode',
                style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold)),
            const SizedBox(height: 8),
            SwitchListTile(
              title: const Text('LED'),
              value: _ledEnabled,
              onChanged: connected
                  ? (v) {
                      setState(() => _ledEnabled = v);
                      _ble.writeFeedbackMode(_buildFeedbackBitmask());
                    }
                  : null,
            ),
            SwitchListTile(
              title: const Text('Vibration'),
              value: _vibEnabled,
              onChanged: connected
                  ? (v) {
                      setState(() => _vibEnabled = v);
                      _ble.writeFeedbackMode(_buildFeedbackBitmask());
                    }
                  : null,
            ),
          ],
        ),
      ),
    );
  }
}
