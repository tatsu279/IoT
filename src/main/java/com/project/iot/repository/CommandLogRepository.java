package com.project.iot.repository;

import com.project.iot.entity.CommandLog;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.stereotype.Repository;
import org.springframework.transaction.annotation.Transactional;

import java.util.List;

@Repository
public interface CommandLogRepository extends JpaRepository<CommandLog, Long> {
    List<CommandLog> findTop10ByDeviceIdOrderBySentAtDesc(String deviceId);
    long countByDeviceId(String deviceId);
    CommandLog findFirstByDeviceIdOrderBySentAtAsc(String deviceId);
    
    @Transactional
    void deleteByDeviceId(String deviceId);
}
