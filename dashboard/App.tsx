import React from 'react';
import { NavigationContainer, DefaultTheme } from '@react-navigation/native';
import { createBottomTabNavigator } from '@react-navigation/bottom-tabs';
import { StatusBar } from 'expo-status-bar';
import { View, Text, StyleSheet } from 'react-native';

import { MqttProvider } from './src/context/MqttContext';
import HomeScreen from './src/screens/HomeScreen';
import LogScreen from './src/screens/LogScreen';
import SettingsScreen from './src/screens/SettingsScreen';

const Tab = createBottomTabNavigator();

const VaayuDarkTheme = {
  ...DefaultTheme,
  colors: {
    ...DefaultTheme.colors,
    background: '#0d1117',
    card: '#161b22',
    text: '#e6edf3',
    border: '#21262d',
    primary: '#2dd4bf',
    notification: '#ff4d4d',
  },
};

function TabBarIcon({ name, focused }: { name: string; focused: boolean }) {
  const icons: Record<string, string> = {
    Home: '⌂',
    Log: '≡',
    Settings: '⚙',
  };
  return (
    <Text style={{ fontSize: 20, color: focused ? '#2dd4bf' : '#6e7681' }}>
      {icons[name] ?? '●'}
    </Text>
  );
}

export default function App() {
  return (
    <MqttProvider>
      <NavigationContainer theme={VaayuDarkTheme}>
        <StatusBar style="light" backgroundColor="#0d1117" />
        <Tab.Navigator
          screenOptions={({ route }) => ({
            headerStyle: { backgroundColor: '#161b22' },
            headerTintColor: '#e6edf3',
            headerTitleStyle: { fontWeight: '700', letterSpacing: 1 },
            tabBarStyle: {
              backgroundColor: '#161b22',
              borderTopColor: '#21262d',
              height: 60,
              paddingBottom: 8,
            },
            tabBarActiveTintColor: '#2dd4bf',
            tabBarInactiveTintColor: '#6e7681',
            tabBarIcon: ({ focused }) => (
              <TabBarIcon name={route.name} focused={focused} />
            ),
          })}
        >
          <Tab.Screen
            name="Home"
            component={HomeScreen}
            options={{ title: 'Vaayu Hub' }}
          />
          <Tab.Screen
            name="Log"
            component={LogScreen}
            options={{ title: 'Event Log' }}
          />
          <Tab.Screen
            name="Settings"
            component={SettingsScreen}
            options={{ title: 'Settings' }}
          />
        </Tab.Navigator>
      </NavigationContainer>
    </MqttProvider>
  );
}

const styles = StyleSheet.create({});
