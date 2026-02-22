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
  Timer? _reconnectTimer;

  String? _deviceId;
  BleConnectionStatus _currentStatus = BleConnectionStatus.disconnected;

  void Function()? onConnected;

  Stream<BleConnectionStatus> get statusStream => _statusController.stream;
  Stream<int> get soundLevelStream => _soundLevelController.stream;

  void _emitStatus(BleConnectionStatus status) {
    _currentStatus = status;
    _statusController.add(status);
  }

  void scan() {
    _emitStatus(BleConnectionStatus.scanning);
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
    _emitStatus(BleConnectionStatus.connecting);
    _connectSub?.cancel();
    _connectSub = _ble
        .connectToDevice(
      id: deviceId,
      servicesWithCharacteristicsToDiscover: {
        serviceUuid: [
          thresholdCharUuid,
          soundLevelCharUuid,
          feedbackModeCharUuid,
          sampleCountCharUuid,
          syncCtrlCharUuid,
          syncDataCharUuid,
        ],
      },
    )
        .listen((update) {
      if (update.connectionState == DeviceConnectionState.connected) {
        _emitStatus(BleConnectionStatus.connected);
        _subscribeSoundLevel();
        onConnected?.call();
      } else if (update.connectionState ==
          DeviceConnectionState.disconnected) {
        _emitStatus(BleConnectionStatus.disconnected);
      }
    }, onError: (_) {
      _emitStatus(BleConnectionStatus.disconnected);
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

  Future<int> readSampleCount() async {
    final char = QualifiedCharacteristic(
      serviceId: serviceUuid,
      characteristicId: sampleCountCharUuid,
      deviceId: _deviceId!,
    );
    final data = await _ble.readCharacteristic(char);
    if (data.length < 4) return 0;
    return ByteData.sublistView(Uint8List.fromList(data))
        .getUint32(0, Endian.little);
  }

  Future<void> writeSyncCtrl(int cmd) async {
    final char = QualifiedCharacteristic(
      serviceId: serviceUuid,
      characteristicId: syncCtrlCharUuid,
      deviceId: _deviceId!,
    );
    await _ble.writeCharacteristicWithResponse(
      char,
      value: Uint8List.fromList([cmd & 0xFF]),
    );
  }

  Stream<List<int>> get syncDataStream {
    final char = QualifiedCharacteristic(
      serviceId: serviceUuid,
      characteristicId: syncDataCharUuid,
      deviceId: _deviceId!,
    );
    return _ble.subscribeToCharacteristic(char);
  }

  void startAutoConnect() {
    _reconnectTimer?.cancel();
    scan();
    _reconnectTimer = Timer.periodic(const Duration(seconds: 30), (_) {
      if (_statusController.isClosed) return;
      if (_currentStatus == BleConnectionStatus.disconnected) {
        scan();
      }
    });
  }

  void stopAutoConnect() {
    _reconnectTimer?.cancel();
    _reconnectTimer = null;
  }

  void disconnect() {
    _reconnectTimer?.cancel();
    _reconnectTimer = null;
    _notifySub?.cancel();
    _connectSub?.cancel();
    _scanSub?.cancel();
    _deviceId = null;
    _emitStatus(BleConnectionStatus.disconnected);
  }

  void dispose() {
    disconnect();
    _statusController.close();
    _soundLevelController.close();
  }
}
