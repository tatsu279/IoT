package com.project.iot.service;

import com.project.iot.entity.Device;
import com.project.iot.entity.CommandLog;
import com.project.iot.repository.DeviceRepository;
import com.project.iot.repository.CommandLogRepository;
import lombok.RequiredArgsConstructor;
import org.eclipse.paho.client.mqttv3.*;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.boot.context.event.ApplicationReadyEvent;
import org.springframework.context.event.EventListener;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.time.LocalDateTime;
import java.util.Optional;

@Service
@RequiredArgsConstructor
public class MqttService {

    @Value("${mqtt.broker}")
    private String brokerUrl;

    @Value("${mqtt.client-id}")
    private String clientId;

    private final DeviceRepository deviceRepository;
    private final CommandLogRepository commandLogRepository;
    private IMqttClient mqttClient;

    // Use ApplicationReadyEvent instead of @PostConstruct to ensure Spring context is fully ready
    @EventListener(ApplicationReadyEvent.class)
    public void init() {
        try {
            mqttClient = new MqttClient(brokerUrl, clientId);
            MqttConnectOptions options = new MqttConnectOptions();
            options.setAutomaticReconnect(true);
            options.setCleanSession(true);
            options.setConnectionTimeout(15);

            mqttClient.setCallback(new MqttCallback() {
                @Override
                public void connectionLost(Throwable cause) {
                    System.out.println("⚠️ MQTT Connection Lost: " + cause.getMessage());
                }

                @Override
                public void messageArrived(String topic, MqttMessage message) {
                    String payload = new String(message.getPayload());
                    System.out.println("📩 MQTT RX: " + topic + " -> " + payload);
                    // Process in a new thread to avoid blocking MQTT client
                    // and to ensure Spring transaction context works
                    try {
                        processAndSave(topic, payload);
                    } catch (Exception e) {
                        System.err.println("❌ Error processing MQTT message: " + e.getMessage());
                        e.printStackTrace();
                    }
                }

                @Override
                public void deliveryComplete(IMqttDeliveryToken token) {}
            });

            mqttClient.connect(options);

            // Subscribe to ESP32 status topics and control topics
            mqttClient.subscribe("mylamp_app/device/+/status", 1);
            mqttClient.subscribe("mylamp_app/device/+/control", 1);
            System.out.println("✅ Connected to MQTT Broker: " + brokerUrl);
            System.out.println("✅ Subscribed to: mylamp_app/device/+/status & mylamp_app/device/+/control");
        } catch (MqttException e) {
            System.err.println("❌ MQTT Connection failed: " + e.getMessage());
            e.printStackTrace();
        }
    }

    @Transactional
    public void processAndSave(String topic, String payload) {
        // topic format from ESP32: mylamp_app/device/{device_id}/status
        String[] parts = topic.split("/");
        if (parts.length == 4 && "status".equals(parts[3]) && "mylamp_app".equals(parts[0])) {
            String deviceId = parts[2]; // e.g. "esp32_relay_A1D908"

            // Parse from ESP32 payload: {"device_id":"...","relay":"ON","online":true}
            boolean isOn = payload.contains("\"ON\"");
            boolean isOnline = payload.contains("\"online\":true");

            Optional<Device> opt = deviceRepository.findByMacAddress(deviceId);
            Device device;
            if (opt.isPresent()) {
                device = opt.get();
                System.out.println("🔍 Found device in DB: " + device.getMacAddress() + " (id=" + device.getId() + ")");
            } else {
                // Auto-register device when it first reports via MQTT
                device = new Device();
                device.setMacAddress(deviceId);
                String shortName = deviceId.length() > 6 ? deviceId.substring(deviceId.length() - 6) : deviceId;
                device.setName("ESP32 IoT " + shortName);
                System.out.println("🆕 Auto-registering new device: " + deviceId);
            }

            boolean stateChanged = false;
            if (device.getId() == null) {
                stateChanged = true; // New device
            }
            
            if (device.isOn() != isOn) {
                device.setOn(isOn);
                stateChanged = true;
            }
            
            if (device.isOnline() != isOnline) {
                device.setOnline(isOnline);
                stateChanged = true;
            }

            // Only issue a DB update if state changed, or if 5 minutes have passed (heartbeat)
            LocalDateTime threshold = LocalDateTime.now().minusMinutes(5);
            boolean needsHeartbeatUpdate = device.getLastActive() == null || device.getLastActive().isBefore(threshold);

            if (stateChanged || needsHeartbeatUpdate) {
                device.setLastActive(LocalDateTime.now());
                Device saved = deviceRepository.save(device);
                System.out.println("💾 DB Updated: " + saved.getMacAddress()
                        + " | relay=" + (isOn ? "ON" : "OFF")
                        + " | online=" + isOnline
                        + " | id=" + saved.getId());
            }
        } else if (parts.length == 4 && "control".equals(parts[3]) && "mylamp_app".equals(parts[0])) {
            String deviceId = parts[2];
            // Extract command from payload if possible
            String command = payload.contains("\"ON\"") ? "ON" : (payload.contains("\"OFF\"") ? "OFF" : "UNKNOWN");
            if (payload.contains("\"command\":\"")) {
                int start = payload.indexOf("\"command\":\"") + 11;
                int end = payload.indexOf("\"", start);
                if (end > start) command = payload.substring(start, end);
            }
            
            CommandLog log;
            if (commandLogRepository.countByDeviceId(deviceId) >= 10) {
                log = commandLogRepository.findFirstByDeviceIdOrderBySentAtAsc(deviceId);
                if (log == null) log = new CommandLog();
            } else {
                log = new CommandLog();
            }
            log.setDeviceId(deviceId);
            log.setCommand(command);
            log.setSentAt(LocalDateTime.now());
            commandLogRepository.save(log);
            System.out.println("📝 Command Logged (Ring Buffered): " + deviceId + " -> " + command);
            
            // Optimistically update device state when command is seen
            Optional<Device> opt = deviceRepository.findByMacAddress(deviceId);
            if (opt.isPresent()) {
                Device device = opt.get();
                if ("ON".equals(command) || "OFF".equals(command)) {
                    device.setOn("ON".equals(command));
                    device.setLastActive(LocalDateTime.now());
                    deviceRepository.save(device);
                }
            }
        }
    }

    public void publishCommand(String deviceId, String command) {
        try {
            // Publish to ESP32's control topic: mylamp_app/device/{device_id}/control
            String topic = "mylamp_app/device/" + deviceId + "/control";
            String payload = String.format("{\"command\":\"%s\",\"timestamp\":%d}", command, System.currentTimeMillis());
            MqttMessage message = new MqttMessage(payload.getBytes());
            message.setQos(1);
            mqttClient.publish(topic, message);
            System.out.println("📤 MQTT Published: " + topic + " -> " + payload);
        } catch (MqttException e) {
            System.err.println("❌ MQTT Publish failed: " + e.getMessage());
            e.printStackTrace();
        }
    }

    public boolean isMqttConnected() {
        return mqttClient != null && mqttClient.isConnected();
    }
}
