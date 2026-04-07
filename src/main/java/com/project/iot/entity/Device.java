package com.project.iot.entity;

import jakarta.persistence.*;
import lombok.Data;
import java.time.LocalDateTime;
import java.util.UUID;

@Entity
@Table(name = "devices")
@Data
public class Device {
    @Id
    @GeneratedValue(strategy = GenerationType.UUID)
    private UUID id;

    @Column(name = "mac_address", unique = true, nullable = false)
    private String macAddress;

    @Column(name = "user_id")
    private Long userId;

    private String name;
    
    @Column(name = "is_on")
    private boolean isOn = false;

    @Column(name = "is_online")
    private boolean isOnline = false;

    @Column(name = "category", length = 50)
    private String category = "LIGHT"; // Default to LIGHT for existing devices

    @Column(name = "last_active")
    private LocalDateTime lastActive;
}

