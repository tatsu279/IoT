package com.dinhquoccuong.lab306.service.impl;

import lombok.AllArgsConstructor;
import org.springframework.stereotype.Service;
import com.dinhquoccuong.lab306.entity.Category;
import com.dinhquoccuong.lab306.repository.CategoryRepository;
import com.dinhquoccuong.lab306.service.CategoryService;

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
        Optional<Category> optionalCategory =
                categoryRepository.findById(categoryId);
        return optionalCategory.get();
    }

    @Override
    public List<Category> getAllCategories() {
        return categoryRepository.findAll();
    }

    @Override
    public Category updateCategory(Category category) {
        Category existingCategory =
                categoryRepository.findById(category.getId()).get();

        existingCategory.setTitle(category.getTitle());
        existingCategory.setDescription(category.getDescription());
        existingCategory.setPhoto(category.getPhoto());
        existingCategory.setProducts(category.getProducts());

        return categoryRepository.save(existingCategory);
    }

    @Override
    public void deleteCategory(Long categoryId) {
        categoryRepository.deleteById(categoryId);
    }
}
