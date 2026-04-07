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
import java.util.Map;
import java.util.Optional;
import java.util.UUID;
import java.util.concurrent.ConcurrentHashMap;

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

    // ── Deduplication ────────────────────────────────────────────────────────
    // Track last logged command per device to prevent duplicates
    private static final long DEDUP_WINDOW_MS = 3000; // 3 seconds
    private final Map<String, DedupEntry> lastCommandMap = new ConcurrentHashMap<>();

    private static class DedupEntry {
        final String command;
        final long timestamp;
        DedupEntry(String command, long timestamp) {
            this.command = command;
            this.timestamp = timestamp;
        }
    }

    private boolean isDuplicate(String deviceId, String command) {
        DedupEntry last = lastCommandMap.get(deviceId);
        long now = System.currentTimeMillis();
        if (last != null && last.command.equals(command) && (now - last.timestamp) < DEDUP_WINDOW_MS) {
            return true;
        }
        lastCommandMap.put(deviceId, new DedupEntry(command, now));
        return false;
    }

    // Track messages published by this backend to avoid echo-logging
    private final Map<String, Long> selfPublished = new ConcurrentHashMap<>();
    private static final long SELF_PUBLISH_WINDOW_MS = 5000;

    // ── MQTT Connection ──────────────────────────────────────────────────────
    @EventListener(ApplicationReadyEvent.class)
    public void init() {
        try {
            // Unique client-id per instance to prevent client-id conflicts
            String uniqueClientId = clientId + "-" + UUID.randomUUID().toString().substring(0, 8);
            mqttClient = new MqttClient(brokerUrl, uniqueClientId);
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

            // Subscribe to ESP32 status topics ONLY
            // Control topic logging is handled directly in the REST/service layer
            // to prevent echo-loop and duplicate logging
            mqttClient.subscribe("mylamp_app/device/+/status", 1);
            System.out.println("✅ Connected to MQTT Broker: " + brokerUrl + " (clientId=" + uniqueClientId + ")");
            System.out.println("✅ Subscribed to: mylamp_app/device/+/status");
        } catch (MqttException e) {
            System.err.println("❌ MQTT Connection failed: " + e.getMessage());
            e.printStackTrace();
        }
    }

    @Transactional
    public void processAndSave(String topic, String payload) {
        String[] parts = topic.split("/");
        if (parts.length != 4 || !"mylamp_app".equals(parts[0])) return;

        String deviceId = parts[2];
        String topicType = parts[3];

        if ("status".equals(topicType)) {
            handleStatusMessage(deviceId, payload);
        }
        // NOTE: We no longer process "control" messages from MQTT subscription.
        // Command logging is done directly in logCommand() called from the controller
        // or publishAndLogCommand(). This prevents:
        // 1. Echo-loop (backend publishing then receiving its own message)
        // 2. QoS 1 redelivery causing duplicates
    }

    // ── Status Message Handler ───────────────────────────────────────────────
    private void handleStatusMessage(String deviceId, String payload) {
        boolean isOn = payload.contains("\"ON\"");
        boolean isOnline = payload.contains("\"online\":true");

        Optional<Device> opt = deviceRepository.findByMacAddress(deviceId);
        Device device;
        if (opt.isPresent()) {
            device = opt.get();
            System.out.println("🔍 Found device: " + device.getMacAddress() + " (id=" + device.getId() + ")");
        } else {
            device = new Device();
            device.setMacAddress(deviceId);
            String shortName = deviceId.length() > 6 ? deviceId.substring(deviceId.length() - 6) : deviceId;
            device.setName("ESP32 IoT " + shortName);
            System.out.println("🆕 Auto-registering new device: " + deviceId);
        }

        boolean stateChanged = (device.getId() == null)
                || (device.isOn() != isOn)
                || (device.isOnline() != isOnline);

        if (device.isOn() != isOn) device.setOn(isOn);
        if (device.isOnline() != isOnline) device.setOnline(isOnline);

        // Only write to DB if state changed or heartbeat needed (5 min)
        LocalDateTime threshold = LocalDateTime.now().minusMinutes(5);
        boolean needsHeartbeat = device.getLastActive() == null || device.getLastActive().isBefore(threshold);

        if (stateChanged || needsHeartbeat) {
            device.setLastActive(LocalDateTime.now());
            Device saved = deviceRepository.save(device);
            System.out.println("💾 Device Updated: " + saved.getMacAddress()
                    + " | relay=" + (isOn ? "ON" : "OFF")
                    + " | online=" + isOnline
                    + " | id=" + saved.getId());
        }
    }

    // ── Command Logging (called directly, NOT from MQTT callback) ────────────
    /**
     * Log a command to the database with deduplication.
     * Uses a simple ring-buffer: keeps max 10 entries per device,
     * deletes the oldest when the limit is reached.
     * 
     * @return true if the command was logged, false if it was a duplicate
     */
    @Transactional
    public boolean logCommand(String deviceId, String command) {
        // Deduplication: skip if same command was logged recently
        if (isDuplicate(deviceId, command)) {
            System.out.println("⏭️ Skipping duplicate command: " + deviceId + " -> " + command);
            return false;
        }

        // Ring buffer: delete oldest if at capacity
        long count = commandLogRepository.countByDeviceId(deviceId);
        if (count >= 10) {
            CommandLog oldest = commandLogRepository.findFirstByDeviceIdOrderBySentAtAsc(deviceId);
            if (oldest != null) {
                commandLogRepository.delete(oldest);
                System.out.println("🗑️ Deleted oldest log (id=" + oldest.getId() + ") for ring buffer");
            }
        }

        // Insert new entry
        CommandLog log = new CommandLog();
        log.setDeviceId(deviceId);
        log.setCommand(command);
        log.setSentAt(LocalDateTime.now());
        commandLogRepository.save(log);
        System.out.println("📝 Command Logged: " + deviceId + " -> " + command);

        // Also optimistically update device state
        Optional<Device> opt = deviceRepository.findByMacAddress(deviceId);
        if (opt.isPresent()) {
            Device device = opt.get();
            if ("ON".equals(command) || "OFF".equals(command)) {
                device.setOn("ON".equals(command));
                device.setLastActive(LocalDateTime.now());
                deviceRepository.save(device);
            }
        }

        return true;
    }

    // ── Publish + Log (single entry point for sending commands) ──────────────
    /**
     * Publish a command to MQTT AND log it to the database.
     * This is the single entry point to ensure exactly-once logging.
     */
    public void publishAndLogCommand(String deviceId, String command) {
        // 1. Log to DB first
        logCommand(deviceId, command);

        // 2. Publish to MQTT
        try {
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

    /**
     * @deprecated Use {@link #publishAndLogCommand(String, String)} instead.
     * Kept for backward compatibility but now delegates to publishAndLogCommand.
     */
    public void publishCommand(String deviceId, String command) {
        publishAndLogCommand(deviceId, command);
    }

    public boolean isMqttConnected() {
        return mqttClient != null && mqttClient.isConnected();
    }
}
