import React, { useEffect, useRef, useState } from 'react';
import {
  FlatList,
  View,
  Text,
  StyleSheet,
  TouchableOpacity,
} from 'react-native';
import { useMqtt } from '../context/MqttContext';

interface LogEntry {
  id: string;
  reason: string;
  timestamp: Date;
}

const MAX_ENTRIES = 50;

export default function LogScreen() {
  const { stateReason } = useMqtt();
  const [log, setLog] = useState<LogEntry[]>([]);
  const prevReason = useRef<string>('');

  useEffect(() => {
    if (stateReason && stateReason !== prevReason.current) {
      prevReason.current = stateReason;
      setLog((prev) => {
        const entry: LogEntry = {
          id: `${Date.now()}-${Math.random()}`,
          reason: stateReason,
          timestamp: new Date(),
        };
        return [entry, ...prev].slice(0, MAX_ENTRIES);
      });
    }
  }, [stateReason]);

  function formatTime(d: Date): string {
    return d.toLocaleTimeString(undefined, {
      hour: '2-digit',
      minute: '2-digit',
      second: '2-digit',
    });
  }

  function formatDate(d: Date): string {
    return d.toLocaleDateString(undefined, {
      day: 'numeric',
      month: 'short',
    });
  }

  const renderItem = ({ item, index }: { item: LogEntry; index: number }) => (
    <View style={[styles.entry, index === 0 && styles.entryLatest]}>
      <View style={styles.timeBadge}>
        <Text style={styles.time}>{formatTime(item.timestamp)}</Text>
        <Text style={styles.date}>{formatDate(item.timestamp)}</Text>
      </View>
      <View style={styles.divider} />
      <Text style={[styles.reason, index === 0 && styles.reasonLatest]}>
        {item.reason}
      </Text>
    </View>
  );

  return (
    <View style={styles.container}>
      {log.length === 0 ? (
        <View style={styles.empty}>
          <Text style={styles.emptyIcon}>📋</Text>
          <Text style={styles.emptyText}>Waiting for hub events…</Text>
          <Text style={styles.emptyHint}>
            Reason strings from the firmware will appear here as they change.
          </Text>
        </View>
      ) : (
        <>
          <View style={styles.header}>
            <Text style={styles.headerText}>
              {log.length} event{log.length !== 1 ? 's' : ''} · newest first
            </Text>
            <TouchableOpacity onPress={() => setLog([])}>
              <Text style={styles.clearBtn}>Clear</Text>
            </TouchableOpacity>
          </View>
          <FlatList
            data={log}
            keyExtractor={(item) => item.id}
            renderItem={renderItem}
            contentContainerStyle={styles.list}
            showsVerticalScrollIndicator={false}
          />
        </>
      )}
    </View>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#0d1117' },

  header: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    paddingHorizontal: 16,
    paddingVertical: 10,
    borderBottomWidth: 1,
    borderBottomColor: '#21262d',
  },
  headerText: { color: '#6e7681', fontSize: 12 },
  clearBtn: { color: '#f87171', fontSize: 13, fontWeight: '600' },

  list: { padding: 12, gap: 8, paddingBottom: 30 },

  entry: {
    backgroundColor: '#161b22',
    borderRadius: 10,
    borderWidth: 1,
    borderColor: '#21262d',
    padding: 12,
    flexDirection: 'row',
    alignItems: 'center',
    gap: 10,
  },
  entryLatest: {
    borderColor: '#2dd4bf',
    backgroundColor: '#0d2420',
  },
  timeBadge: { alignItems: 'center', minWidth: 64 },
  time: { color: '#2dd4bf', fontSize: 13, fontWeight: '700', fontVariant: ['tabular-nums'] },
  date: { color: '#6e7681', fontSize: 10, marginTop: 2 },
  divider: { width: 1, height: '100%', backgroundColor: '#21262d' },
  reason: { flex: 1, color: '#8b949e', fontSize: 14, lineHeight: 20 },
  reasonLatest: { color: '#e6edf3', fontWeight: '600' },

  empty: {
    flex: 1,
    alignItems: 'center',
    justifyContent: 'center',
    gap: 10,
    paddingHorizontal: 40,
  },
  emptyIcon: { fontSize: 48, marginBottom: 8 },
  emptyText: { color: '#e6edf3', fontSize: 18, fontWeight: '700', textAlign: 'center' },
  emptyHint: { color: '#6e7681', fontSize: 13, textAlign: 'center', lineHeight: 20 },
});
