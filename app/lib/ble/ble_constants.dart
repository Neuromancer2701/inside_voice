import 'package:flutter_reactive_ble/flutter_reactive_ble.dart';

const String deviceName = 'InsideVoice';

final Uuid serviceUuid =
    Uuid.parse('4f490000-2ff1-4a5e-a683-4de2c5a10100');
final Uuid thresholdCharUuid =
    Uuid.parse('4f490001-2ff1-4a5e-a683-4de2c5a10100');
final Uuid soundLevelCharUuid =
    Uuid.parse('4f490002-2ff1-4a5e-a683-4de2c5a10100');
final Uuid feedbackModeCharUuid =
    Uuid.parse('4f490003-2ff1-4a5e-a683-4de2c5a10100');

final Uuid sampleCountCharUuid =
    Uuid.parse('4f490004-2ff1-4a5e-a683-4de2c5a10100');
final Uuid syncCtrlCharUuid =
    Uuid.parse('4f490005-2ff1-4a5e-a683-4de2c5a10100');
final Uuid syncDataCharUuid =
    Uuid.parse('4f490006-2ff1-4a5e-a683-4de2c5a10100');

const int feedbackModeLed = 1 << 0;
const int feedbackModeVibration = 1 << 1;
