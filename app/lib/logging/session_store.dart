import 'dart:io';

import 'package:path_provider/path_provider.dart';
import 'package:share_plus/share_plus.dart';

class SessionInfo {
  final String path;
  final String filename;
  final int sizeBytes;
  final DateTime modified;

  SessionInfo({
    required this.path,
    required this.filename,
    required this.sizeBytes,
    required this.modified,
  });

  String get sizeFormatted {
    if (sizeBytes < 1024) return '$sizeBytes B';
    if (sizeBytes < 1024 * 1024) {
      return '${(sizeBytes / 1024).toStringAsFixed(1)} KB';
    }
    return '${(sizeBytes / (1024 * 1024)).toStringAsFixed(1)} MB';
  }
}

class SessionStore {
  static Future<Directory> logDirectory() async {
    final ext = await getExternalStorageDirectory();
    final dir = Directory('${ext!.path}/logs');
    if (!await dir.exists()) {
      await dir.create(recursive: true);
    }
    return dir;
  }

  static Future<List<SessionInfo>> listSessions() async {
    final dir = await logDirectory();
    final files = dir
        .listSync()
        .whereType<File>()
        .where((f) => f.path.endsWith('.csv'))
        .toList();

    files.sort((a, b) => b.statSync().modified.compareTo(a.statSync().modified));

    return files.map((f) {
      final stat = f.statSync();
      return SessionInfo(
        path: f.path,
        filename: f.uri.pathSegments.last,
        sizeBytes: stat.size,
        modified: stat.modified,
      );
    }).toList();
  }

  static Future<void> deleteSession(String path) async {
    final file = File(path);
    if (await file.exists()) {
      await file.delete();
    }
  }

  static Future<void> shareSession(String path) async {
    await Share.shareXFiles([XFile(path)]);
  }
}
