import 'dart:async';
import 'dart:io';

import 'package:flutter/foundation.dart';
import 'package:flutter_foreground_task/flutter_foreground_task.dart';
import 'package:intl/intl.dart';

import '../ble/ble_service.dart';
import 'session_store.dart';

class RecordingService {
  RecordingService._();
  static final RecordingService instance = RecordingService._();

  final ValueNotifier<bool> isRecording = ValueNotifier(false);
  final ValueNotifier<Duration> elapsed = ValueNotifier(Duration.zero);

  IOSink? _sink;
  StreamSubscription<int>? _levelSub;
  Timer? _elapsedTimer;
  DateTime? _startTime;
  int _sampleCount = 0;
  String? _currentPath;

  Future<void> startRecording(BleService ble) async {
    if (isRecording.value) return;

    final dir = await SessionStore.logDirectory();
    final stamp = DateFormat('yyyyMMdd_HHmmss').format(DateTime.now());
    final file = File('${dir.path}/iv_$stamp.csv');
    _sink = file.openWrite();
    _sink!.writeln('timestamp_ms,db');
    _currentPath = file.path;
    _sampleCount = 0;

    _startTime = DateTime.now();
    _elapsedTimer = Timer.periodic(const Duration(seconds: 1), (_) {
      elapsed.value = DateTime.now().difference(_startTime!);
    });

    _levelSub = ble.soundLevelStream.listen((level) {
      final ms = DateTime.now().millisecondsSinceEpoch;
      _sink?.writeln('$ms,$level');
      _sampleCount++;
      if (_sampleCount % 100 == 0) {
        _sink?.flush();
      }
    });

    await FlutterForegroundTask.startService(
      notificationTitle: 'InsideVoice Recording',
      notificationText: 'Recording dB data...',
    );

    isRecording.value = true;
  }

  Future<String?> stopRecording() async {
    if (!isRecording.value) return null;

    isRecording.value = false;
    _elapsedTimer?.cancel();
    _elapsedTimer = null;
    elapsed.value = Duration.zero;

    await _levelSub?.cancel();
    _levelSub = null;

    await _sink?.flush();
    await _sink?.close();
    _sink = null;

    await FlutterForegroundTask.stopService();

    final path = _currentPath;
    _currentPath = null;
    return path;
  }
}
