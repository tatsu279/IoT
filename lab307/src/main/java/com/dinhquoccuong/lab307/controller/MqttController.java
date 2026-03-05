package com.dinhquoccuong.lab307.controller;

import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;
import com.dinhquoccuong.lab307.service.MqttPublisherService;

@RestController
@RequestMapping("/api/mqtt")
public class MqttController {
    private final MqttPublisherService mqttPublisherService;

    public MqttController(MqttPublisherService mqttPublisherService) {
        this.mqttPublisherService = mqttPublisherService;
    }

    @GetMapping("/publish")
    public String publish() {
        mqttPublisherService.publish("Hi from the IoT application");
        return "Message published!";
    }
}
