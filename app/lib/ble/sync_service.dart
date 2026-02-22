import 'dart:async';
import 'dart:io';
import 'dart:typed_data';

import 'package:flutter/foundation.dart';
import 'package:intl/intl.dart';

import 'ble_service.dart';
import '../logging/session_store.dart';

enum SyncState { idle, syncing, done, error }

class SyncService {
  SyncService._();
  static final SyncService instance = SyncService._();

  final ValueNotifier<SyncState> state = ValueNotifier(SyncState.idle);
  DateTime? lastSyncTime;

  Future<void> syncIfNeeded(BleService ble) async {
    if (state.value == SyncState.syncing) return;

    try {
      final count = await ble.readSampleCount();
      if (count == 0) return;

      state.value = SyncState.syncing;

      final syncWallTime = DateTime.now();
      final records = <({int uptimeMs, int db})>[];

      // Subscribe to sync data notifications
      final completer = Completer<void>();
      StreamSubscription? sub;

      sub = ble.syncDataStream.listen((data) {
        if (data.length < 5) return;
        // Check sentinel
        if (data[0] == 0xFF &&
            data[1] == 0xFF &&
            data[2] == 0xFF &&
            data[3] == 0xFF &&
            data[4] == 0xFF) {
          sub?.cancel();
          if (!completer.isCompleted) completer.complete();
          return;
        }
        final uptimeMs = ByteData.sublistView(Uint8List.fromList(data))
            .getUint32(0, Endian.little);
        final db = data[4];
        records.add((uptimeMs: uptimeMs, db: db));
      }, onError: (e) {
        sub?.cancel();
        if (!completer.isCompleted) completer.completeError(e);
      });

      // Start streaming
      await ble.writeSyncCtrl(0x01);

      // Wait for sentinel (timeout after 30s)
      await completer.future.timeout(const Duration(seconds: 30));

      // Write 0x02 to clear cache
      await ble.writeSyncCtrl(0x02);

      // Convert uptime_ms to wall time and save CSV
      if (records.isNotEmpty) {
        final finalUptime = records.last.uptimeMs;
        final dir = await SessionStore.logDirectory();
        final stamp = DateFormat('yyyyMMdd_HHmmss').format(syncWallTime);
        final file = File('${dir.path}/iv_sync_$stamp.csv');
        final sink = file.openWrite();
        sink.writeln('timestamp_ms,db');
        for (final r in records) {
          final wallMs = syncWallTime.millisecondsSinceEpoch -
              (finalUptime - r.uptimeMs);
          sink.writeln('$wallMs,${r.db}');
        }
        await sink.flush();
        await sink.close();
      }

      lastSyncTime = DateTime.now();
      state.value = SyncState.done;

      // Reset to idle after a few seconds
      await Future.delayed(const Duration(seconds: 3));
      if (state.value == SyncState.done) {
        state.value = SyncState.idle;
      }
    } catch (e) {
      state.value = SyncState.error;
      // Reset to idle after showing error
      await Future.delayed(const Duration(seconds: 3));
      if (state.value == SyncState.error) {
        state.value = SyncState.idle;
      }
    }
  }
}
