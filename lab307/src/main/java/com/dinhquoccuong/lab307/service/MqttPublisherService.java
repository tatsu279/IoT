package com.dinhquoccuong.lab307.service;

import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.messaging.MessageChannel;
import org.springframework.messaging.support.MessageBuilder;
import org.springframework.stereotype.Service;

@Service
public class MqttPublisherService {
    @Autowired
    private MessageChannel mqttOutboundChannel;

    @Value("/test/topic")
    private String defaultTopic;

    public void publish(String message) {
        mqttOutboundChannel.send(
            MessageBuilder.withPayload(message)
                .setHeader("mqtt_topic", defaultTopic)
                .build()
        );
    }
}
