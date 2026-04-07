package com.project.iot.controller;

import com.project.iot.entity.Device;
import com.project.iot.repository.DeviceRepository;
import com.project.iot.service.MqttService;
import lombok.RequiredArgsConstructor;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.time.LocalDateTime;
import java.util.List;
import java.util.Map;
import java.util.Optional;

@RestController
@RequiredArgsConstructor
public class DeviceController {

    private final DeviceRepository deviceRepository;
    private final com.project.iot.repository.CommandLogRepository commandLogRepository;
    private final MqttService mqttService;

    // ── GET /devices ── List all devices (used by Flutter HomeScreen)
    @GetMapping("/devices")
    public ResponseEntity<List<Device>> getAllDevices() {
        return ResponseEntity.ok(deviceRepository.findAll());
    }

    // ── POST /devices/register-qr ── Register device by QR scan (used by Flutter)
    @PostMapping("/devices/register-qr")
    public ResponseEntity<?> registerQR(@RequestBody Map<String, String> payload) {
        String mac = payload.get("mac_address");
        String name = payload.getOrDefault("name", "Smart Lamp");
        String location = payload.getOrDefault("location", "Room");

        if (mac == null || mac.isEmpty()) {
            return ResponseEntity.badRequest().body(Map.of("error", "mac_address is required"));
        }

        Optional<Device> existOpt = deviceRepository.findByMacAddress(mac);
        Device device;
        if (existOpt.isPresent()) {
            device = existOpt.get();
            device.setName(name);
        } else {
            device = new Device();
            device.setMacAddress(mac);
            device.setName(name);
        }
        device.setLastActive(LocalDateTime.now());
        deviceRepository.save(device);

        return ResponseEntity.ok(Map.of(
            "message", "Device registered successfully",
            "device_id", device.getId().toString(),
            "device", device
        ));
    }

    // ── GET /devices/{id} ── Get single device
    @GetMapping("/devices/{id}")
    public ResponseEntity<?> getDevice(@PathVariable String id) {
        Optional<Device> opt = deviceRepository.findByMacAddress(id);
        if (opt.isEmpty()) {
            try {
                opt = deviceRepository.findById(java.util.UUID.fromString(id));
            } catch (Exception ignored) {}
        }
        return opt.<ResponseEntity<?>>map(ResponseEntity::ok)
                .orElseGet(() -> ResponseEntity.notFound().build());
    }

    // ── POST /api/devices/provision ── Legacy endpoint
    @PostMapping("/api/devices/provision")
    public ResponseEntity<?> provisionDevice(@RequestBody Map<String, String> payload) {
        return registerQR(payload);
    }

    // ── POST /api/devices/{mac}/toggle ── Send ON/OFF via MQTT
    @PostMapping("/api/devices/{mac}/toggle")
    public ResponseEntity<?> toggleDevice(@PathVariable String mac, @RequestBody Map<String, String> payload) {
        String command = payload.get("command");
        // publishAndLogCommand handles BOTH publishing to MQTT AND logging to DB
        mqttService.publishAndLogCommand(mac, command);
        return ResponseEntity.ok(Map.of("message", "Command published and logged"));
    }

    // ── POST /commands/{deviceId} ── Send command (used by Flutter)
    @PostMapping("/commands/{deviceId}")
    public ResponseEntity<?> sendCommand(@PathVariable String deviceId, @RequestBody Map<String, Object> payload) {
        String cmd = (String) payload.get("cmd");
        // publishAndLogCommand handles BOTH publishing to MQTT AND logging to DB
        // This is the single entry point - no echo-loop duplication
        mqttService.publishAndLogCommand(deviceId, cmd);
        return ResponseEntity.ok(Map.of("message", "Command sent and logged", "cmd", cmd));
    }

    // ── DELETE /devices/{id} ── Delete single device
    @DeleteMapping("/devices/{id}")
    public ResponseEntity<?> deleteDevice(@PathVariable String id) {
        Optional<Device> opt = deviceRepository.findByMacAddress(id);
        if (opt.isPresent()) {
            // Also clean up command logs for this device
            commandLogRepository.deleteByDeviceId(id);
            deviceRepository.delete(opt.get());
            return ResponseEntity.ok(Map.of("message", "Device deleted successfully"));
        }
        return ResponseEntity.notFound().build();
    }

    // ── GET /commands/{deviceId} ── Command history
    @GetMapping("/commands/{deviceId}")
    public ResponseEntity<?> getCommandHistory(@PathVariable String deviceId) {
        return ResponseEntity.ok(commandLogRepository.findTop10ByDeviceIdOrderBySentAtDesc(deviceId));
    }

    // ── GET /health ── Check system status (MQTT + DB)
    @GetMapping("/health")
    public ResponseEntity<?> health() {
        List<Device> devices = deviceRepository.findAll();
        long totalLogs = commandLogRepository.count();
        return ResponseEntity.ok(Map.of(
            "status", "UP",
            "mqtt_connected", mqttService.isMqttConnected(),
            "devices_count", devices.size(),
            "command_logs_count", totalLogs,
            "devices", devices
        ));
    }
}
