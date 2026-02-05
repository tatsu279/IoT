package com.dinhquoccuong.lab306.repository;

import org.springframework.data.jpa.repository.JpaRepository;
import com.dinhquoccuong.lab306.entity.Product;

public interface ProductRepository extends JpaRepository<Product, Long> {
}
