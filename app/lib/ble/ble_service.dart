import 'dart:async';
import 'dart:typed_data';

import 'package:flutter_reactive_ble/flutter_reactive_ble.dart';

import 'ble_constants.dart';

enum BleConnectionStatus { disconnected, scanning, connecting, connected }

class BleService {
  final FlutterReactiveBle _ble = FlutterReactiveBle();

  final _statusController =
      StreamController<BleConnectionStatus>.broadcast();
  final _soundLevelController = StreamController<int>.broadcast();

  StreamSubscription? _scanSub;
  StreamSubscription? _connectSub;
  StreamSubscription? _notifySub;

  String? _deviceId;

  Stream<BleConnectionStatus> get statusStream => _statusController.stream;
  Stream<int> get soundLevelStream => _soundLevelController.stream;

  void scan() {
    _statusController.add(BleConnectionStatus.scanning);
    _scanSub?.cancel();
    _scanSub = _ble
        .scanForDevices(withServices: [])
        .where((d) => d.name == deviceName)
        .listen((device) {
      _scanSub?.cancel();
      _connect(device.id);
    });
  }

  void _connect(String deviceId) {
    _deviceId = deviceId;
    _statusController.add(BleConnectionStatus.connecting);
    _connectSub?.cancel();
    _connectSub = _ble
        .connectToDevice(
      id: deviceId,
      servicesWithCharacteristicsToDiscover: {
        serviceUuid: [
          thresholdCharUuid,
          soundLevelCharUuid,
          feedbackModeCharUuid,
        ],
      },
    )
        .listen((update) {
      if (update.connectionState == DeviceConnectionState.connected) {
        _statusController.add(BleConnectionStatus.connected);
        _subscribeSoundLevel();
      } else if (update.connectionState ==
          DeviceConnectionState.disconnected) {
        _statusController.add(BleConnectionStatus.disconnected);
      }
    }, onError: (_) {
      _statusController.add(BleConnectionStatus.disconnected);
    });
  }

  void _subscribeSoundLevel() {
    final char = QualifiedCharacteristic(
      serviceId: serviceUuid,
      characteristicId: soundLevelCharUuid,
      deviceId: _deviceId!,
    );
    _notifySub?.cancel();
    _notifySub = _ble.subscribeToCharacteristic(char).listen((data) {
      if (data.isNotEmpty) {
        _soundLevelController.add(data[0]);
      }
    });
  }

  Future<int> readThreshold() async {
    final char = QualifiedCharacteristic(
      serviceId: serviceUuid,
      characteristicId: thresholdCharUuid,
      deviceId: _deviceId!,
    );
    final data = await _ble.readCharacteristic(char);
    return data.isNotEmpty ? data[0] : 70;
  }

  Future<void> writeThreshold(int value) async {
    final char = QualifiedCharacteristic(
      serviceId: serviceUuid,
      characteristicId: thresholdCharUuid,
      deviceId: _deviceId!,
    );
    await _ble.writeCharacteristicWithResponse(
      char,
      value: Uint8List.fromList([value.clamp(0, 255)]),
    );
  }

  Future<int> readFeedbackMode() async {
    final char = QualifiedCharacteristic(
      serviceId: serviceUuid,
      characteristicId: feedbackModeCharUuid,
      deviceId: _deviceId!,
    );
    final data = await _ble.readCharacteristic(char);
    return data.isNotEmpty ? data[0] : 0x03;
  }

  Future<void> writeFeedbackMode(int bitmask) async {
    final char = QualifiedCharacteristic(
      serviceId: serviceUuid,
      characteristicId: feedbackModeCharUuid,
      deviceId: _deviceId!,
    );
    await _ble.writeCharacteristicWithResponse(
      char,
      value: Uint8List.fromList([bitmask & 0xFF]),
    );
  }

  void disconnect() {
    _notifySub?.cancel();
    _connectSub?.cancel();
    _scanSub?.cancel();
    _deviceId = null;
    _statusController.add(BleConnectionStatus.disconnected);
  }

  void dispose() {
    disconnect();
    _statusController.close();
    _soundLevelController.close();
  }
}
