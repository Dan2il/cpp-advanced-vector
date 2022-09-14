#pragma once
#include <cassert>
#include <cstdlib>
#include <memory>
#include <new>
#include <utility>

template <typename T>
class RawMemory {
   public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity) : buffer_(Allocate(capacity)), capacity_(capacity) {}

    ~RawMemory() { Deallocate(buffer_); }

    T* operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept { return const_cast<RawMemory&>(*this) + offset; }

    const T& operator[](size_t index) const noexcept { return const_cast<RawMemory&>(*this)[index]; }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept { return buffer_; }

    T* GetAddress() noexcept { return buffer_; }

    size_t Capacity() const { return capacity_; }
    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory& rhs) = delete;
    RawMemory(RawMemory&& other) noexcept { Swap(other); }
    RawMemory& operator=(RawMemory&& rhs) noexcept {
        if (this != &rhs) {
            Swap(rhs);
        }
        return *this;
    }

   private:
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n) { return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr; }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept { operator delete(buf); }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector {
   public:
    using iterator = T*;
    using const_iterator = const T*;

    iterator begin() noexcept;
    iterator end() noexcept;
    const_iterator begin() const noexcept;
    const_iterator end() const noexcept;
    const_iterator cbegin() const noexcept;
    const_iterator cend() const noexcept;

    Vector() = default;
    explicit Vector(size_t size);
    Vector(const Vector& other);
    Vector(Vector&& other) noexcept;

    Vector& operator=(const Vector& rhs);
    Vector& operator=(Vector&& other) noexcept;
    const T& operator[](size_t index) const noexcept;
    T& operator[](size_t index) noexcept;

    template <typename U>
    void PushBack(U&& value);
    void PushBack(const T& value);
    void PopBack() noexcept;
    void Swap(Vector& other) noexcept;
    void Reserve(size_t new_capacity);
    void Resize(size_t new_size);

    template <typename... Args>
    T& EmplaceBack(Args&&... args);
    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args);
    iterator Insert(const_iterator pos, const T& value);
    iterator Insert(const_iterator pos, T&& value);
    size_t Size() const noexcept;
    size_t Capacity() const noexcept;
    iterator Erase(const_iterator pos);

    ~Vector();

   private:
    RawMemory<T> data_;
    size_t size_ = 0;

    void Destroy(T* buf) noexcept;
    void DestroyN(T* buf, size_t n) noexcept;
};

// ------------------------ || ------------------------

template <typename T>
Vector<T>::Vector(size_t size) : data_(size), size_(size) {
    std::uninitialized_value_construct_n(data_.GetAddress(), size);
}

template <typename T>
Vector<T>::Vector(const Vector& other) : data_(other.size_), size_(other.size_) {
    std::uninitialized_copy_n(other.data_.GetAddress(), other.size_, data_.GetAddress());
}

template <typename T>
Vector<T>::Vector(Vector&& other) noexcept : data_(RawMemory<T>(std::move(other.data_))), size_(other.size_) {
    other.size_ = 0;
}

template <typename T>
void Vector<T>::Swap(Vector& other) noexcept {
    data_.Swap(other.data_);
    std::swap(size_, other.size_);
}

template <typename T>
void Vector<T>::Reserve(size_t new_capacity) {
    if (new_capacity > Capacity()) {
        RawMemory<T> new_data(new_capacity);
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        } else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        DestroyN(data_.GetAddress(), size_);
        data_.Swap(new_data);
    }
}

template <typename T>
void Vector<T>::Resize(size_t new_size) {
    if (size_ > new_size) {
        std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
        size_ = new_size;
    } else if (size_ < new_size) {
        Reserve(new_size);
        std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
        size_ = new_size;
    }
}
template <typename T>
size_t Vector<T>::Size() const noexcept {
    return size_;
}

template <typename T>
size_t Vector<T>::Capacity() const noexcept {
    return data_.Capacity();
}

template <typename T>
void Vector<T>::PushBack(const T& value) {
    if (size_ == Capacity()) {
        RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
        new_data[size_] = value;
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        } else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
    } else {
        data_[size_] = value;
    }
    ++size_;
}

template <typename T>
template <typename U>
void Vector<T>::PushBack(U&& value) {
    if (Capacity() > size_) {
        new (data_.GetAddress() + size_) T(std::forward<U>(value));
        ++size_;
    } else {
        const size_t new_size = size_ + 1;
        const size_t new_capacity = std::max(Capacity() * 2, new_size);
        RawMemory<T> new_data(new_capacity);
        new (new_data.GetAddress() + size_) T(std::forward<U>(value));
        try {
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<U>) {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
            } else {
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
        } catch (...) {
            std::destroy_n(new_data.GetAddress() + size_, 1);
            throw;
        }
        std::destroy_n(data_.GetAddress(), size_);
        ++size_;
        data_.Swap(new_data);
    }
}

template <typename T>
template <typename... Args>
T& Vector<T>::EmplaceBack(Args&&... args) {
    auto index = size_;
    if (size_ == Capacity()) {
        RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
        new (new_data.GetAddress() + size_) T(std::forward<Args>(args)...);
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        } else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
    } else {
        new (data_.GetAddress() + size_) T(std::forward<Args>(args)...);
    }
    ++size_;
    return data_[index];
}

template <typename T>
template <typename... Args>
typename Vector<T>::iterator Vector<T>::Emplace(const_iterator pos, Args&&... args) {
    assert(pos >= begin() && pos <= end());
    auto offset = pos - begin();
    if (Capacity() > size_) {
        if (pos != end()) {
            T value_to_emplace(std::forward<Args>(args)...);
            std::uninitialized_move_n(std::next(begin(), size_ - 1), 1, end());
            std::move_backward(std::next(begin(), offset), std::next(begin(), size_ - 1),
                               std::next(begin(), size_));
            data_[offset] = std::move(value_to_emplace);
        } else {
            new (end()) T(std::forward<Args>(args)...);
        }
        ++size_;
    } else {
        RawMemory<T> new_data(std::max(Capacity() * 2, size_ + 1));
        new (new_data.GetAddress() + offset) T(std::forward<Args>(args)...);
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(begin(), offset, new_data.GetAddress());
            std::uninitialized_move_n(std::next(begin(), offset), size_ - offset,
                                      new_data.GetAddress() + offset + 1);
        } else {
            std::uninitialized_copy_n(begin(), offset, new_data.GetAddress());
            std::uninitialized_copy_n(std::next(begin(), offset), size_ - offset,
                                      new_data.GetAddress() + offset + 1);
        }
        std::destroy_n(data_.GetAddress(), size_);
        ++size_;
        data_.Swap(new_data);
    }
    return std::next(begin(), offset);
}

template <typename T>
typename Vector<T>::iterator Vector<T>::Erase(const_iterator pos) {
    assert(pos >= begin() && pos < end());
    iterator it = const_cast<iterator>(pos);
    size_t distance = pos - begin();
    std::move(it + 1, end(), it);
    std::destroy_at(data_.GetAddress() + size_);
    size_--;
    return std::next(begin(), distance);
}

template <typename T>
typename Vector<T>::iterator Vector<T>::Insert(const_iterator pos, const T& value) {
    return Emplace(pos, value);
}

template <typename T>
typename Vector<T>::iterator Vector<T>::Insert(const_iterator pos, T&& value) {
    return Emplace(pos, std::forward<T>(value));
}

template <typename T>
void Vector<T>::PopBack() noexcept {
    if (size_ == 0) {
        return;
    }
    std::destroy_at(data_.GetAddress() + size_);
    size_--;
}

template <typename T>
Vector<T>& Vector<T>::operator=(const Vector& rhs) {
    if (this != &rhs) {
        if (rhs.size_ > data_.Capacity()) {
            Vector rhs_copy(rhs);
            Swap(rhs_copy);
        } else {
            if (rhs.Size() >= size_) {
                for (size_t i = 0; i != size_; ++i) {
                    data_[i] = rhs.data_[i];
                }
                std::uninitialized_copy_n(rhs.data_.GetAddress() + size_, rhs.size_ - size_,
                                          data_.GetAddress() + size_);
                size_ = rhs.size_;
            } else {
                size_t i = 0;
                for (; i != rhs.size_; ++i) {
                    data_[i] = rhs.data_[i];
                }
                std::destroy_n(data_.GetAddress() + i, size_ - rhs.size_);
                size_ = i;
            }
        }
    }
    return *this;
}

template <typename T>
void Vector<T>::Destroy(T* buf) noexcept {
    buf->~T();
}

template <typename T>
void Vector<T>::DestroyN(T* buf, size_t n) noexcept {
    for (size_t i = 0; i != n; ++i) {
        Destroy(buf + i);
    }
}

template <typename T>
typename Vector<T>::iterator Vector<T>::begin() noexcept {
    return data_.GetAddress();
}

template <typename T>
typename Vector<T>::iterator Vector<T>::end() noexcept {
    return data_.GetAddress() + size_;
}

template <typename T>
typename Vector<T>::const_iterator Vector<T>::begin() const noexcept {
    return data_.GetAddress();
}

template <typename T>
typename Vector<T>::const_iterator Vector<T>::end() const noexcept {
    return data_.GetAddress() + size_;
}

template <typename T>
typename Vector<T>::const_iterator Vector<T>::cbegin() const noexcept {
    return data_.GetAddress();
}

template <typename T>
typename Vector<T>::const_iterator Vector<T>::cend() const noexcept {
    return data_.GetAddress() + size_;
}

template <typename T>
Vector<T>& Vector<T>::operator=(Vector&& other) noexcept {
    if (this != &other) {
        Swap(other);
    }
    return *this;
}

template <typename T>
const T& Vector<T>::operator[](size_t index) const noexcept {
    return const_cast<Vector&>(*this)[index];
}

template <typename T>
T& Vector<T>::operator[](size_t index) noexcept {
    assert(index < size_);
    return data_[index];
}

template <typename T>
Vector<T>::~Vector() {
    DestroyN(data_.GetAddress(), size_);
}
