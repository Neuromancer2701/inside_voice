import 'package:flutter/material.dart';
import 'package:intl/intl.dart';

import '../logging/session_store.dart';

class SessionsScreen extends StatefulWidget {
  const SessionsScreen({super.key});

  @override
  State<SessionsScreen> createState() => _SessionsScreenState();
}

class _SessionsScreenState extends State<SessionsScreen> {
  List<SessionInfo> _sessions = [];
  bool _loading = true;

  @override
  void initState() {
    super.initState();
    _loadSessions();
  }

  Future<void> _loadSessions() async {
    setState(() => _loading = true);
    final sessions = await SessionStore.listSessions();
    setState(() {
      _sessions = sessions;
      _loading = false;
    });
  }

  Future<void> _deleteSession(SessionInfo session) async {
    final confirm = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('Delete recording?'),
        content: Text(session.filename),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(ctx, false),
            child: const Text('Cancel'),
          ),
          TextButton(
            onPressed: () => Navigator.pop(ctx, true),
            child: const Text('Delete'),
          ),
        ],
      ),
    );
    if (confirm == true) {
      await SessionStore.deleteSession(session.path);
      _loadSessions();
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('Recordings')),
      body: _loading
          ? const Center(child: CircularProgressIndicator())
          : _sessions.isEmpty
              ? const Center(child: Text('No recordings yet'))
              : RefreshIndicator(
                  onRefresh: _loadSessions,
                  child: ListView.builder(
                    itemCount: _sessions.length,
                    itemBuilder: (context, index) {
                      final s = _sessions[index];
                      final dateStr =
                          DateFormat('yyyy-MM-dd HH:mm:ss').format(s.modified);
                      return Dismissible(
                        key: Key(s.path),
                        direction: DismissDirection.endToStart,
                        background: Container(
                          color: Colors.red,
                          alignment: Alignment.centerRight,
                          padding: const EdgeInsets.only(right: 16),
                          child:
                              const Icon(Icons.delete, color: Colors.white),
                        ),
                        confirmDismiss: (_) async {
                          await _deleteSession(s);
                          return false; // we handle removal in _deleteSession
                        },
                        child: ListTile(
                          title: Text(s.filename),
                          subtitle: Text('$dateStr  ·  ${s.sizeFormatted}'),
                          trailing: IconButton(
                            icon: const Icon(Icons.share),
                            onPressed: () =>
                                SessionStore.shareSession(s.path),
                          ),
                        ),
                      );
                    },
                  ),
                ),
    );
  }
}
