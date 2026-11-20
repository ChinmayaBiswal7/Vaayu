import React, { useState } from 'react';
import {
  View,
  Text,
  ScrollView,
  StyleSheet,
  TouchableOpacity,
  Switch,
} from 'react-native';
import { useMqtt } from '../context/MqttContext';
import GaugeCircle from '../components/GaugeCircle';
import AlarmBanner from '../components/AlarmBanner';
import StatusCard from '../components/StatusCard';

export default function HomeScreen() {
  const {
    roomTemp,
    roomHumidity,
    presence,
    distance,
    fanSpeed,
    acSetpoint,
    acState,
    exhaustState,
    stateReason,
    alarm,
    connected,
    publish,
    setOverride,
  } = useMqtt();

  const [alarmDismissed, setAlarmDismissed] = useState(false);
  const [isManual, setIsManual] = useState(false);

  const showAlarm = !!alarm && alarm !== 'NONE' && alarm !== 'CLEARED' && alarm.trim() !== '' && !alarmDismissed;

  function handleOverrideToggle(manual: boolean) {
    setIsManual(manual);
    setOverride(manual ? 'MANUAL' : 'AUTO');
  }

  const fanVal = fanSpeed ?? 0;

  return (
    <ScrollView
      style={styles.container}
      contentContainerStyle={styles.content}
      showsVerticalScrollIndicator={false}
    >
      {/* Connection status */}
      <View style={styles.connRow}>
        <View
          style={[
            styles.connDot,
            { backgroundColor: connected ? '#2dd4bf' : '#6e7681' },
          ]}
        />
        <Text style={styles.connText}>
          {connected ? 'Connected to hub' : 'Not connected'}
        </Text>
      </View>

      {/* Alarm */}
      {showAlarm && (
        <AlarmBanner
          message={alarm!}
          onDismiss={() => setAlarmDismissed(true)}
        />
      )}

      {/* Fan speed gauge */}
      <View style={styles.gaugeSection}>
        <View style={styles.gaugeWrapper}>
          <GaugeCircle value={fanVal} size={180} strokeWidth={16} />
          <View style={styles.gaugeCenter}>
            <Text style={styles.gaugeValue}>{Math.round(fanVal)}%</Text>
            <Text style={styles.gaugeLabel}>Fan</Text>
          </View>
        </View>

        {/* Temp + Humidity under gauge */}
        <View style={styles.metricRow}>
          <View style={styles.metricBox}>
            <Text style={styles.metricValue}>
              {roomTemp !== null ? `${roomTemp.toFixed(1)}°C` : '—'}
            </Text>
            <Text style={styles.metricLabel}>Temperature</Text>
          </View>
          <View style={styles.metricDivider} />
          <View style={styles.metricBox}>
            <Text style={styles.metricValue}>
              {roomHumidity !== null ? `${roomHumidity.toFixed(0)}%` : '—'}
            </Text>
            <Text style={styles.metricLabel}>Humidity</Text>
          </View>
        </View>
      </View>

      {/* Status cards */}
      <View style={styles.cardGrid}>
        <StatusCard
          label="Presence"
          value={presence ? 'Detected' : 'Empty'}
          active={presence}
          icon="👤"
          style={styles.cardHalf}
        />
        <StatusCard
          label="Distance"
          value={distance !== null ? `${distance.toFixed(1)} m` : '—'}
          icon="📡"
          style={styles.cardHalf}
        />
        <StatusCard
          label="Air Conditioning"
          value={acState ? `ON  ·  ${acSetpoint ?? '—'}°C` : 'OFF'}
          active={acState}
          icon="❄️"
          style={styles.cardFull}
          accentColor="#60a5fa"
        />
        <StatusCard
          label="Exhaust Fan"
          value={exhaustState ? 'Running' : 'Off'}
          active={exhaustState}
          icon="💨"
          style={styles.cardFull}
        />
      </View>

      {/* Reason string */}
      <View style={styles.reasonCard}>
        <Text style={styles.reasonTitle}>System Reason</Text>
        <Text style={styles.reasonText}>{stateReason}</Text>
      </View>

      {/* Override toggle */}
      <View style={styles.overrideCard}>
        <View style={styles.overrideRow}>
          <Text style={styles.overrideLabel}>
            {isManual ? '🔧 Manual Override' : '🤖 Auto Mode'}
          </Text>
          <Switch
            value={isManual}
            onValueChange={handleOverrideToggle}
            trackColor={{ false: '#374151', true: '#0d9488' }}
            thumbColor={isManual ? '#2dd4bf' : '#6e7681'}
          />
        </View>
        {isManual && (
          <View style={styles.manualControls}>
            <Text style={styles.manualHint}>
              Tap a command to send it manually:
            </Text>
            <View style={styles.buttonRow}>
              {[0, 25, 50, 75, 100].map((speed) => (
                <TouchableOpacity
                  key={speed}
                  style={[
                    styles.speedBtn,
                    Math.round(fanVal) === speed && styles.speedBtnActive,
                  ]}
                  onPress={() => publish('vaayu/cmd/fan_speed', String(speed))}
                >
                  <Text
                    style={[
                      styles.speedBtnText,
                      Math.round(fanVal) === speed && styles.speedBtnTextActive,
                    ]}
                  >
                    {speed}%
                  </Text>
                </TouchableOpacity>
              ))}
            </View>
            <View style={styles.buttonRow}>
              {[24, 25, 26, 27, 28].map((sp) => (
                <TouchableOpacity
                  key={sp}
                  style={[
                    styles.acBtn,
                    acSetpoint === sp && styles.acBtnActive,
                  ]}
                  onPress={() => publish('vaayu/cmd/ac_setpoint', String(sp))}
                >
                  <Text
                    style={[
                      styles.speedBtnText,
                      acSetpoint === sp && styles.speedBtnTextActive,
                    ]}
                  >
                    {sp}°
                  </Text>
                </TouchableOpacity>
              ))}
              <TouchableOpacity
                style={[styles.acBtn, acState && styles.acBtnActive]}
                onPress={() => publish('vaayu/cmd/ac_state', acState ? '0' : '1')}
              >
                <Text
                  style={[styles.speedBtnText, acState && styles.speedBtnTextActive]}
                >
                  {acState ? '❄ ON' : '❄ OFF'}
                </Text>
              </TouchableOpacity>
            </View>
          </View>
        )}
      </View>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#0d1117' },
  content: { paddingHorizontal: 16, paddingVertical: 12, gap: 12 },

  connRow: { flexDirection: 'row', alignItems: 'center', gap: 8, marginBottom: 2 },
  connDot: { width: 8, height: 8, borderRadius: 4 },
  connText: { color: '#6e7681', fontSize: 12 },

  gaugeSection: {
    backgroundColor: '#161b22',
    borderRadius: 16,
    borderWidth: 1,
    borderColor: '#21262d',
    paddingVertical: 20,
    alignItems: 'center',
    gap: 16,
  },
  gaugeWrapper: { width: 180, height: 180, alignItems: 'center', justifyContent: 'center' },
  gaugeCenter: {
    position: 'absolute',
    alignItems: 'center',
  },
  gaugeValue: {
    color: '#e6edf3',
    fontSize: 36,
    fontWeight: '800',
    letterSpacing: -1,
  },
  gaugeLabel: { color: '#6e7681', fontSize: 13, fontWeight: '600' },

  metricRow: { flexDirection: 'row', width: '80%' },
  metricBox: { flex: 1, alignItems: 'center' },
  metricDivider: { width: 1, backgroundColor: '#21262d', marginVertical: 4 },
  metricValue: { color: '#e6edf3', fontSize: 22, fontWeight: '700' },
  metricLabel: { color: '#6e7681', fontSize: 11, marginTop: 2 },

  cardGrid: { gap: 10 },
  cardHalf: { flex: 1 },
  cardFull: {},

  reasonCard: {
    backgroundColor: '#161b22',
    borderRadius: 12,
    borderWidth: 1,
    borderColor: '#21262d',
    padding: 14,
  },
  reasonTitle: {
    color: '#6e7681',
    fontSize: 11,
    fontWeight: '600',
    textTransform: 'uppercase',
    letterSpacing: 0.8,
    marginBottom: 6,
  },
  reasonText: { color: '#2dd4bf', fontSize: 15, fontWeight: '600', lineHeight: 22 },

  overrideCard: {
    backgroundColor: '#161b22',
    borderRadius: 12,
    borderWidth: 1,
    borderColor: '#21262d',
    padding: 14,
    marginBottom: 20,
  },
  overrideRow: { flexDirection: 'row', alignItems: 'center', justifyContent: 'space-between' },
  overrideLabel: { color: '#e6edf3', fontSize: 15, fontWeight: '600' },

  manualControls: { marginTop: 14, gap: 10 },
  manualHint: { color: '#6e7681', fontSize: 12, marginBottom: 4 },

  buttonRow: { flexDirection: 'row', flexWrap: 'wrap', gap: 8 },
  speedBtn: {
    backgroundColor: '#21262d',
    borderRadius: 8,
    paddingHorizontal: 14,
    paddingVertical: 8,
  },
  speedBtnActive: { backgroundColor: '#0d9488' },
  speedBtnText: { color: '#6e7681', fontWeight: '600', fontSize: 13 },
  speedBtnTextActive: { color: '#e6edf3' },
  acBtn: {
    backgroundColor: '#21262d',
    borderRadius: 8,
    paddingHorizontal: 12,
    paddingVertical: 8,
  },
  acBtnActive: { backgroundColor: '#1d4ed8' },
});
