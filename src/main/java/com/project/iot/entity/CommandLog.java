package com.project.iot.entity;

import jakarta.persistence.*;
import lombok.Data;
import java.time.LocalDateTime;

@Entity
@Table(name = "command_logs")
@Data
public class CommandLog {
    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    @Column(name = "device_id", length = 100)
    private String deviceId;

    @Column(length = 50)
    private String command;

    @Column(name = "sent_at")
    private LocalDateTime sentAt;
}
