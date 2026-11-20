import React, {
  createContext,
  useContext,
  useEffect,
  useRef,
  useState,
  useCallback,
} from 'react';
import mqtt, { MqttClient } from 'mqtt';
import AsyncStorage from '@react-native-async-storage/async-storage';

// ─── Types ───────────────────────────────────────────────────────────────────

export interface VaayuState {
  // Sensors
  roomTemp: number | null;
  roomHumidity: number | null;
  presence: boolean;
  distance: number | null;
  // Actuators
  fanSpeed: number | null;    // 0-100
  acSetpoint: number | null;  // °C
  acState: boolean;
  exhaustState: boolean;
  // Status
  stateReason: string;
  alarm: string | null;
  // Connection
  connected: boolean;
  brokerUrl: string;
}

export interface MqttContextValue extends VaayuState {
  publish: (topic: string, payload: string) => void;
  setOverride: (mode: 'MANUAL' | 'AUTO') => void;
  setBrokerIp: (ip: string) => void;
  reconnect: () => void;
}

// ─── Defaults ────────────────────────────────────────────────────────────────

const getInitialBrokerIp = (): string => {
  if (typeof window !== 'undefined' && window.location && window.location.hostname) {
    return window.location.hostname || 'localhost';
  }
  return '192.168.29.208';
};

const DEFAULT_BROKER_IP = getInitialBrokerIp();

const DEFAULT_STATE: VaayuState = {
  roomTemp: null,
  roomHumidity: null,
  presence: false,
  distance: null,
  fanSpeed: null,
  acSetpoint: null,
  acState: false,
  exhaustState: false,
  stateReason: 'Connecting…',
  alarm: null,
  connected: false,
  brokerUrl: `ws://${DEFAULT_BROKER_IP}:9001`,
};

const SUBSCRIBE_TOPICS = [
  'vaayu/room/temp',
  'vaayu/room/humidity',
  'vaayu/room/presence',
  'vaayu/room/distance',
  'vaayu/fan/speed',
  'vaayu/ac/setpoint',
  'vaayu/ac/state',
  'vaayu/exhaust/state',
  'vaayu/state/reason',
  'vaayu/alarm',
] as const;

// ─── Context ─────────────────────────────────────────────────────────────────

const MqttContext = createContext<MqttContextValue>({
  ...DEFAULT_STATE,
  publish: () => {},
  setOverride: () => {},
  setBrokerIp: () => {},
  reconnect: () => {},
});

export function MqttProvider({ children }: { children: React.ReactNode }) {
  const [state, setState] = useState<VaayuState>(DEFAULT_STATE);
  const clientRef = useRef<MqttClient | null>(null);
  const reconnectTimer = useRef<ReturnType<typeof setTimeout> | null>(null);
  const brokerIpRef = useRef<string>(DEFAULT_BROKER_IP);

  // ── Load saved broker IP ─────────────────────────────────────────────────
  useEffect(() => {
    AsyncStorage.getItem('brokerIp').then((ip) => {
      if (ip) {
        brokerIpRef.current = ip;
        setState((s) => ({ ...s, brokerUrl: `ws://${ip}:9001` }));
      }
      connect(ip ?? DEFAULT_BROKER_IP);
    });
    return () => {
      clientRef.current?.end(true);
      if (reconnectTimer.current) clearTimeout(reconnectTimer.current);
    };
  }, []);

  // ── MQTT connect ─────────────────────────────────────────────────────────
  const connect = useCallback((ip: string) => {
    if (clientRef.current) {
      clientRef.current.end(true);
      clientRef.current = null;
    }

    const url = `ws://${ip}:9001`;
    console.log('[MQTT] Connecting to', url);

    const client = mqtt.connect(url, {
      clientId: `vaayu-dash-${Math.random().toString(16).slice(2, 8)}`,
      keepalive: 30,
      reconnectPeriod: 0, // manual reconnect
      connectTimeout: 8000,
    });

    clientRef.current = client;

    client.on('connect', () => {
      console.log('[MQTT] Connected');
      setState((s) => ({ ...s, connected: true, brokerUrl: url, stateReason: 'Connected' }));
      SUBSCRIBE_TOPICS.forEach((topic) => client.subscribe(topic));
    });

    client.on('message', (topic, payload) => {
      const msg = payload.toString().trim();
      setState((s) => applyMessage(s, topic, msg));
    });

    client.on('error', (err) => {
      console.warn('[MQTT] Error', err.message);
    });

    client.on('close', () => {
      console.log('[MQTT] Disconnected — retrying in 5s');
      setState((s) => ({ ...s, connected: false, stateReason: 'Disconnected — retrying…' }));
      reconnectTimer.current = setTimeout(() => connect(brokerIpRef.current), 5000);
    });

    client.on('offline', () => {
      setState((s) => ({ ...s, connected: false }));
    });
  }, []);

  // ── Message → state ──────────────────────────────────────────────────────
  function applyMessage(s: VaayuState, topic: string, msg: string): VaayuState {
    switch (topic) {
      case 'vaayu/room/temp':
        return { ...s, roomTemp: parseFloat(msg) || s.roomTemp };
      case 'vaayu/room/humidity':
        return { ...s, roomHumidity: parseFloat(msg) || s.roomHumidity };
      case 'vaayu/room/presence':
        return { ...s, presence: msg === '1' || msg.toLowerCase() === 'true' };
      case 'vaayu/room/distance':
        return { ...s, distance: parseFloat(msg) || s.distance };
      case 'vaayu/fan/speed':
        return { ...s, fanSpeed: Math.min(100, Math.max(0, parseFloat(msg))) };
      case 'vaayu/ac/setpoint':
        return { ...s, acSetpoint: parseFloat(msg) || s.acSetpoint };
      case 'vaayu/ac/state':
        return { ...s, acState: msg === '1' || msg.toLowerCase() === 'on' };
      case 'vaayu/exhaust/state':
        return { ...s, exhaustState: msg === '1' || msg.toLowerCase() === 'on' };
      case 'vaayu/state/reason':
        return { ...s, stateReason: msg };
      case 'vaayu/alarm':
        return { ...s, alarm: (!msg || msg === 'CLEAR' || msg === 'CLEARED' || msg === 'NONE') ? null : msg };
      default:
        return s;
    }
  }

  // ── Publish ──────────────────────────────────────────────────────────────
  const publish = useCallback((topic: string, payload: string) => {
    clientRef.current?.publish(topic, payload);
  }, []);

  const setOverride = useCallback((mode: 'MANUAL' | 'AUTO') => {
    publish('vaayu/cmd/override', mode);
  }, [publish]);

  const setBrokerIp = useCallback((ip: string) => {
    brokerIpRef.current = ip;
    AsyncStorage.setItem('brokerIp', ip);
    setState((s) => ({ ...s, brokerUrl: `ws://${ip}:9001` }));
    if (reconnectTimer.current) clearTimeout(reconnectTimer.current);
    connect(ip);
  }, [connect]);

  const reconnect = useCallback(() => {
    if (reconnectTimer.current) clearTimeout(reconnectTimer.current);
    connect(brokerIpRef.current);
  }, [connect]);

  return (
    <MqttContext.Provider value={{ ...state, publish, setOverride, setBrokerIp, reconnect }}>
      {children}
    </MqttContext.Provider>
  );
}

export function useMqtt() {
  return useContext(MqttContext);
}
