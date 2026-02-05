package com.dinhquoccuong.lab304.repository;


import org.springframework.data.jpa.repository.JpaRepository;

import com.dinhquoccuong.lab304.entity.Product;

public interface ProductRepository extends JpaRepository<Product, Long> {
}

