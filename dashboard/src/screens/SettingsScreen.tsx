import React, { useEffect, useState } from 'react';
import {
  View,
  Text,
  TextInput,
  TouchableOpacity,
  ScrollView,
  StyleSheet,
  Alert,
  KeyboardAvoidingView,
  Platform,
} from 'react-native';
import AsyncStorage from '@react-native-async-storage/async-storage';
import { useMqtt } from '../context/MqttContext';

const STORAGE_KEYS = {
  brokerIp: 'brokerIp',
  tempFanOn: 'tempFanOn',
  tempAcOn: 'tempAcOn',
  tempComfort: 'tempComfort',
};

const DEFAULT_SETTINGS = {
  brokerIp: '192.168.4.1',
  tempFanOn: '24',
  tempAcOn: '26',
  tempComfort: '25',
};

interface FieldProps {
  label: string;
  hint?: string;
  value: string;
  onChangeText: (v: string) => void;
  keyboardType?: 'default' | 'numeric' | 'decimal-pad' | 'url';
  placeholder?: string;
}

function Field({ label, hint, value, onChangeText, keyboardType = 'default', placeholder }: FieldProps) {
  return (
    <View style={styles.field}>
      <Text style={styles.fieldLabel}>{label}</Text>
      {hint ? <Text style={styles.fieldHint}>{hint}</Text> : null}
      <TextInput
        style={styles.input}
        value={value}
        onChangeText={onChangeText}
        keyboardType={keyboardType}
        placeholder={placeholder}
        placeholderTextColor="#6e7681"
        autoCapitalize="none"
        autoCorrect={false}
      />
    </View>
  );
}

export default function SettingsScreen() {
  const { brokerUrl, connected, setBrokerIp, reconnect } = useMqtt();

  const [brokerIp, setBrokerIpLocal] = useState(DEFAULT_SETTINGS.brokerIp);
  const [tempFanOn, setTempFanOn] = useState(DEFAULT_SETTINGS.tempFanOn);
  const [tempAcOn, setTempAcOn] = useState(DEFAULT_SETTINGS.tempAcOn);
  const [tempComfort, setTempComfort] = useState(DEFAULT_SETTINGS.tempComfort);
  const [saved, setSaved] = useState(false);

  useEffect(() => {
    AsyncStorage.multiGet(Object.values(STORAGE_KEYS)).then((pairs) => {
      const map = Object.fromEntries(pairs.map(([k, v]) => [k, v]));
      if (map.brokerIp) setBrokerIpLocal(map.brokerIp);
      if (map.tempFanOn) setTempFanOn(map.tempFanOn);
      if (map.tempAcOn) setTempAcOn(map.tempAcOn);
      if (map.tempComfort) setTempComfort(map.tempComfort);
    });
  }, []);

  async function handleSave() {
    // Validate
    const ipRegex = /^(\d{1,3}\.){3}\d{1,3}$/;
    if (!ipRegex.test(brokerIp)) {
      Alert.alert('Invalid IP', 'Enter a valid IP address (e.g., 192.168.4.1)');
      return;
    }
    const fan = parseFloat(tempFanOn);
    const ac = parseFloat(tempAcOn);
    const comfort = parseFloat(tempComfort);
    if (isNaN(fan) || isNaN(ac) || isNaN(comfort)) {
      Alert.alert('Invalid values', 'Temperature thresholds must be numbers.');
      return;
    }
    if (fan >= ac) {
      Alert.alert('Invalid thresholds', 'Fan-on temp must be lower than AC-on temp.');
      return;
    }

    await AsyncStorage.multiSet([
      [STORAGE_KEYS.brokerIp, brokerIp],
      [STORAGE_KEYS.tempFanOn, tempFanOn],
      [STORAGE_KEYS.tempAcOn, tempAcOn],
      [STORAGE_KEYS.tempComfort, tempComfort],
    ]);

    setBrokerIp(brokerIp);
    setSaved(true);
    setTimeout(() => setSaved(false), 2500);
  }

  function handleReset() {
    Alert.alert('Reset to defaults?', 'All settings will be restored.', [
      { text: 'Cancel', style: 'cancel' },
      {
        text: 'Reset',
        style: 'destructive',
        onPress: () => {
          setBrokerIpLocal(DEFAULT_SETTINGS.brokerIp);
          setTempFanOn(DEFAULT_SETTINGS.tempFanOn);
          setTempAcOn(DEFAULT_SETTINGS.tempAcOn);
          setTempComfort(DEFAULT_SETTINGS.tempComfort);
        },
      },
    ]);
  }

  return (
    <KeyboardAvoidingView
      style={{ flex: 1 }}
      behavior={Platform.OS === 'ios' ? 'padding' : undefined}
    >
      <ScrollView style={styles.container} contentContainerStyle={styles.content}>
        {/* Connection info */}
        <View style={styles.infoCard}>
          <View style={styles.infoRow}>
            <View
              style={[
                styles.connDot,
                { backgroundColor: connected ? '#2dd4bf' : '#6e7681' },
              ]}
            />
            <Text style={styles.infoText}>
              {connected ? `Connected · ${brokerUrl}` : `Not connected · ${brokerUrl}`}
            </Text>
          </View>
          <TouchableOpacity onPress={reconnect} style={styles.reconnectBtn}>
            <Text style={styles.reconnectText}>↺ Reconnect</Text>
          </TouchableOpacity>
        </View>

        {/* Broker */}
        <View style={styles.section}>
          <Text style={styles.sectionTitle}>MQTT Broker</Text>
          <Field
            label="Broker IP Address"
            hint="IP of the ESP32-S3 access point or router"
            value={brokerIp}
            onChangeText={setBrokerIpLocal}
            keyboardType="url"
            placeholder="192.168.4.1"
          />
          <Text style={styles.portNote}>WebSocket port 9001 is used automatically.</Text>
        </View>

        {/* Temperature thresholds */}
        <View style={styles.section}>
          <Text style={styles.sectionTitle}>Temperature Thresholds</Text>
          <Text style={styles.sectionHint}>
            These are for display reference. The actual control thresholds are configured in the firmware (config.h). Saving these publishes them to the hub if supported.
          </Text>
          <Field
            label="Fan turns on above (°C)"
            value={tempFanOn}
            onChangeText={setTempFanOn}
            keyboardType="decimal-pad"
            placeholder="24"
          />
          <Field
            label="AC turns on above (°C)"
            value={tempAcOn}
            onChangeText={setTempAcOn}
            keyboardType="decimal-pad"
            placeholder="26"
          />
          <Field
            label="Comfort setpoint (°C)"
            value={tempComfort}
            onChangeText={setTempComfort}
            keyboardType="decimal-pad"
            placeholder="25"
          />
        </View>

        {/* Buttons */}
        <TouchableOpacity
          style={[styles.saveBtn, saved && styles.saveBtnDone]}
          onPress={handleSave}
          activeOpacity={0.8}
        >
          <Text style={styles.saveBtnText}>{saved ? '✓ Saved!' : 'Save & Connect'}</Text>
        </TouchableOpacity>

        <TouchableOpacity
          style={styles.resetBtn}
          onPress={handleReset}
          activeOpacity={0.8}
        >
          <Text style={styles.resetBtnText}>Reset to Defaults</Text>
        </TouchableOpacity>

        <Text style={styles.version}>Vaayu Hub Dashboard  v1.0.0</Text>
      </ScrollView>
    </KeyboardAvoidingView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#0d1117' },
  content: { padding: 16, gap: 16, paddingBottom: 40 },

  infoCard: {
    backgroundColor: '#161b22',
    borderRadius: 12,
    borderWidth: 1,
    borderColor: '#21262d',
    padding: 14,
    gap: 10,
  },
  infoRow: { flexDirection: 'row', alignItems: 'center', gap: 8 },
  connDot: { width: 8, height: 8, borderRadius: 4 },
  infoText: { color: '#6e7681', fontSize: 12, flex: 1 },
  reconnectBtn: {
    alignSelf: 'flex-start',
    backgroundColor: '#21262d',
    borderRadius: 8,
    paddingHorizontal: 14,
    paddingVertical: 7,
  },
  reconnectText: { color: '#2dd4bf', fontSize: 13, fontWeight: '600' },

  section: {
    backgroundColor: '#161b22',
    borderRadius: 12,
    borderWidth: 1,
    borderColor: '#21262d',
    padding: 16,
    gap: 14,
  },
  sectionTitle: {
    color: '#e6edf3',
    fontSize: 14,
    fontWeight: '700',
    textTransform: 'uppercase',
    letterSpacing: 0.6,
  },
  sectionHint: { color: '#6e7681', fontSize: 12, lineHeight: 18 },
  portNote: { color: '#6e7681', fontSize: 11, marginTop: -8 },

  field: { gap: 4 },
  fieldLabel: { color: '#8b949e', fontSize: 13, fontWeight: '600' },
  fieldHint: { color: '#6e7681', fontSize: 11 },
  input: {
    backgroundColor: '#0d1117',
    borderWidth: 1,
    borderColor: '#30363d',
    borderRadius: 8,
    color: '#e6edf3',
    fontSize: 15,
    paddingHorizontal: 12,
    paddingVertical: 10,
    fontFamily: Platform.OS === 'ios' ? 'Menlo' : 'monospace',
  },

  saveBtn: {
    backgroundColor: '#0d9488',
    borderRadius: 12,
    padding: 16,
    alignItems: 'center',
  },
  saveBtnDone: { backgroundColor: '#065f46' },
  saveBtnText: { color: '#fff', fontSize: 16, fontWeight: '700' },

  resetBtn: {
    borderRadius: 12,
    padding: 14,
    alignItems: 'center',
    borderWidth: 1,
    borderColor: '#374151',
  },
  resetBtnText: { color: '#6e7681', fontSize: 14, fontWeight: '600' },

  version: { color: '#21262d', fontSize: 11, textAlign: 'center', marginTop: 8 },
});
