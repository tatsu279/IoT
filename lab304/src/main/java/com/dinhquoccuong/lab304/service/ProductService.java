package com.dinhquoccuong.lab304.service;

import com.dinhquoccuong.lab304.entity.Product;
import  java.util.List;

public interface ProductService {

    Product createProduct(Product product);

    Product getProductById(Long productId);

    List<Product> getAllProducts();

    Product updateProduct(Product product);

    void deleteProduct(Long productId);
}
