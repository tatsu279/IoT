package com.dinhquoccuong.lab305.service;

import java.util.List;
import com.dinhquoccuong.lab305.entity.Category;

public interface CategoryService {

    Category createCategory(Category category);

    Category getCategoryById(Long categoryId);

    List<Category> getAllCategorys();

    Category updateCategory(Category category);

    void deleteCategory(Long categoryId);
}
