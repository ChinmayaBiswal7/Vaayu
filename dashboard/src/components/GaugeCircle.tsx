import React, { useRef } from 'react';
import { View, Animated, StyleSheet } from 'react-native';
import Svg, { Circle, Defs, LinearGradient, Stop } from 'react-native-svg';

interface GaugeCircleProps {
  value: number;      // 0-100
  size?: number;
  strokeWidth?: number;
  label?: string;
}

export default function GaugeCircle({
  value,
  size = 160,
  strokeWidth = 14,
}: GaugeCircleProps) {
  const radius = (size - strokeWidth) / 2;
  const circumference = 2 * Math.PI * radius;
  const clampedValue = Math.min(100, Math.max(0, value));

  // Arc goes from 135° to 405° (270° sweep, bottom-left to bottom-right)
  const SWEEP_DEG = 270;
  const offset = circumference - (clampedValue / 100) * (circumference * (SWEEP_DEG / 360));

  // Colour: teal at low → warm amber at high
  function getColor(v: number): string {
    if (v < 40) return '#2dd4bf';
    if (v < 70) return '#fbbf24';
    return '#f87171';
  }

  const color = getColor(clampedValue);

  return (
    <Svg width={size} height={size} viewBox={`0 0 ${size} ${size}`}>
      {/* Background track */}
      <Circle
        cx={size / 2}
        cy={size / 2}
        r={radius}
        stroke="#21262d"
        strokeWidth={strokeWidth}
        fill="none"
        strokeDasharray={`${circumference * (SWEEP_DEG / 360)} ${circumference}`}
        strokeDashoffset={0}
        strokeLinecap="round"
        rotation={135}
        originX={size / 2}
        originY={size / 2}
      />
      {/* Value arc */}
      <Circle
        cx={size / 2}
        cy={size / 2}
        r={radius}
        stroke={color}
        strokeWidth={strokeWidth}
        fill="none"
        strokeDasharray={`${circumference * (SWEEP_DEG / 360)} ${circumference}`}
        strokeDashoffset={offset}
        strokeLinecap="round"
        rotation={135}
        originX={size / 2}
        originY={size / 2}
      />
    </Svg>
  );
}
