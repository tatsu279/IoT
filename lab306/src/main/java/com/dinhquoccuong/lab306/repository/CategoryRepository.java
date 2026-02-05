package com.dinhquoccuong.lab306.repository;

import org.springframework.data.jpa.repository.JpaRepository;
import com.dinhquoccuong.lab306.entity.Category;

public interface CategoryRepository extends JpaRepository<Category, Long> {
}
