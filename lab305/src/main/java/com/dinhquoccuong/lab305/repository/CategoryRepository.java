package com.dinhquoccuong.lab305.repository;

import org.springframework.data.jpa.repository.JpaRepository;
import com.dinhquoccuong.lab305.entity.Category;

public interface CategoryRepository extends JpaRepository<Category, Long> {
}