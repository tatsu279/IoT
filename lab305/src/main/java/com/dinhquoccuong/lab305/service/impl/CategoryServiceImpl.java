package com.dinhquoccuong.lab305.service.impl;

import lombok.AllArgsConstructor;
import org.springframework.stereotype.Service;

import com.dinhquoccuong.lab305.entity.Category;
import com.dinhquoccuong.lab305.repository.CategoryRepository;
import com.dinhquoccuong.lab305.service.CategoryService;

import java.util.List;
import java.util.Optional;

@Service
@AllArgsConstructor
public class CategoryServiceImpl implements CategoryService {

    private CategoryRepository categoryRepository;

    @Override
    public Category createCategory(Category category) {
        return categoryRepository.save(category);
    }

    @Override
    public Category getCategoryById(Long categoryId) {
        Optional<Category> category = categoryRepository.findById(categoryId);
        return category.get();
    }

    @Override
    public List<Category> getAllCategorys() {
        return categoryRepository.findAll();
    }

    @Override
    public Category updateCategory(Category category) {
        Category existing = categoryRepository.findById(category.getCategoryId()).get();


        existing.setName(category.getName());
        existing.setDescription(category.getDescription());
        existing.setParentCategory(category.getParentCategory());
        existing.setParentCategory(category);

        Category updatedCategory = categoryRepository.save(existing);

        return updatedCategory;
    }

    @Override
    public void deleteCategory(Long categoryId) {
        categoryRepository.deleteById(categoryId);
    }
}
