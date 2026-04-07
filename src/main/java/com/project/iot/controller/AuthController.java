package com.project.iot.controller;

import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.util.Map;
import java.util.UUID;

@RestController
@RequestMapping("/auth")
public class AuthController {

    // Simple auth without Spring Security - suitable for IoT demo project
    private static final String DEMO_USERNAME = "admin";
    private static final String DEMO_PASSWORD = "admin123";

    @PostMapping("/login")
    public ResponseEntity<?> login(@RequestBody Map<String, String> payload) {
        String username = payload.get("username");
        String password = payload.get("password");

        if (DEMO_USERNAME.equals(username) && DEMO_PASSWORD.equals(password)) {
            // Generate a simple token (in production use JWT)
            String token = UUID.randomUUID().toString();
            return ResponseEntity.ok(Map.of(
                "token", token,
                "username", username,
                "message", "Login successful"
            ));
        }
        return ResponseEntity.status(401).body(Map.of("error", "Invalid credentials"));
    }

    @GetMapping("/me")
    public ResponseEntity<?> getMe(@RequestHeader(value = "Authorization", required = false) String authHeader) {
        if (authHeader != null && authHeader.startsWith("Bearer ")) {
            return ResponseEntity.ok(Map.of(
                "username", DEMO_USERNAME,
                "role", "admin"
            ));
        }
        return ResponseEntity.status(401).body(Map.of("error", "Unauthorized"));
    }
}
