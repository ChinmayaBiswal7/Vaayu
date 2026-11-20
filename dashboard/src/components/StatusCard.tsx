import React from 'react';
import { View, Text, StyleSheet, ViewStyle } from 'react-native';

interface StatusCardProps {
  label: string;
  value: string;
  active?: boolean;
  icon?: string;
  style?: ViewStyle;
  accentColor?: string;
}

export default function StatusCard({
  label,
  value,
  active,
  icon,
  style,
  accentColor = '#2dd4bf',
}: StatusCardProps) {
  const borderColor = active ? accentColor : '#21262d';

  return (
    <View style={[styles.card, { borderColor }, style]}>
      {icon ? <Text style={styles.icon}>{icon}</Text> : null}
      <View style={styles.textGroup}>
        <Text style={styles.label}>{label}</Text>
        <Text style={[styles.value, active && { color: accentColor }]}>{value}</Text>
      </View>
      {active !== undefined && (
        <View
          style={[
            styles.dot,
            { backgroundColor: active ? accentColor : '#374151' },
          ]}
        />
      )}
    </View>
  );
}

const styles = StyleSheet.create({
  card: {
    backgroundColor: '#161b22',
    borderWidth: 1.5,
    borderRadius: 12,
    padding: 14,
    flexDirection: 'row',
    alignItems: 'center',
    gap: 12,
  },
  icon: {
    fontSize: 22,
  },
  textGroup: {
    flex: 1,
  },
  label: {
    color: '#6e7681',
    fontSize: 11,
    fontWeight: '600',
    textTransform: 'uppercase',
    letterSpacing: 0.8,
    marginBottom: 2,
  },
  value: {
    color: '#e6edf3',
    fontSize: 16,
    fontWeight: '700',
  },
  dot: {
    width: 10,
    height: 10,
    borderRadius: 5,
  },
});
