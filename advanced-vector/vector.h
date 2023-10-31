#pragma once
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <memory>
#include <new>
#include <utility>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    RawMemory(const RawMemory&) = delete;
    RawMemory operator=(const RawMemory&) = delete;

    RawMemory(RawMemory&& other) noexcept {
        buffer_ = std::exchange(other.buffer_, nullptr);
        capacity_ = std::exchange(other.capacity_, 0);
    }

    RawMemory& operator=(RawMemory&& other) noexcept {
        buffer_ = std::exchange(other.buffer_, nullptr);
        capacity_ = std::exchange(other.capacity_, 0);
        return *this;
    }

    T* operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }


    /// <summary>
    /// Transfer (move if this is safely possible/copy otherwise) nmb_elements from source to destination. 
    /// </summary>
    /// <param name="source"></param>
    /// <param name="nmb_elements"></param>
    /// <param name="destination"></param>
    static void CopyOrMoveData(T* source , size_t nmb_elements, T* destination) {
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(source, nmb_elements, destination);
        }
        else {
            std::uninitialized_copy_n(source, nmb_elements, destination);
        }
    }



private:
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};


template <typename T>
class Vector {
public:

    using iterator = T*;
    using const_iterator = const T*;

    iterator begin() noexcept {
        return data_.GetAddress();
    }

    iterator end() noexcept {
        return data_.GetAddress() + size_;
    }

    const_iterator begin() const noexcept {
        return data_.GetAddress();
    }

    const_iterator end() const noexcept {
        return data_.GetAddress() + size_;
    }

    const_iterator cbegin() const noexcept {
        return data_.GetAddress();
    }

    const_iterator cend() const noexcept {
        return data_.GetAddress() + size_;
    }


    Vector() = default;

    Vector(size_t size)
        : data_(size)
        , size_(size)
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size_);
    }

    Vector(const Vector& other)
        : data_(other.size_)
        , size_(other.size_)
    {
        std::uninitialized_copy_n(other.data_.GetAddress(), size_, data_.GetAddress());
    }


    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }

    Vector(Vector&& other) noexcept
        : data_(std::move(other.data_)) {
        size_ = std::exchange(other.size_, 0);
    }

    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                // Copy-&-Swap
                Vector rhs_copy(rhs);
                Swap(rhs_copy);
            }
            else {
                size_t copies_count = std::min(rhs.size_, size_);
                std::copy_n(rhs.data_.GetAddress(), copies_count, data_.GetAddress());
                if (rhs.size_ < size_) {
                    std::destroy_n(data_ + rhs.size_, size_ - rhs.size_);
                }
                else {
                    std::uninitialized_copy_n(rhs.data_.GetAddress() + size_, rhs.size_ - size_, data_.GetAddress() + size_);
                }
                size_ = rhs.size_;
            }
        }
        return *this;
    }

    Vector& operator=(Vector&& rhs) noexcept {
        data_ = std::move(rhs.data_);
        size_ = std::exchange(rhs.size_, 0);
        return *this;
    }

    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }
        RawMemory<T> new_data(new_capacity);

        // Конструируем элементы в new_data, копируя их из data_
        RawMemory<T>::CopyOrMoveData(data_.GetAddress(), size_, new_data.GetAddress());

        // Разрушаем элементы в data_
        std::destroy_n(data_.GetAddress(), size_);
        // Избавляемся от старой сырой памяти, обменивая её на новую
        data_.Swap(new_data);
    }


    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }


    void Resize(size_t new_size) {
        if (new_size <= size_) {
            std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
            size_ = new_size;
        }
        else {
            Reserve(new_size);
            std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
            size_ = new_size;
        }
    }

    void PushBack(const T& value) {
        EmplaceBack(value);
    }

    void PushBack(T&& value) {
        EmplaceBack(std::move(value));
    }


    template <typename...Args>
    T& EmplaceBack(Args&&...args) {
        return *Emplace(cend(), std::forward<Args>(args)...);
    }



    void PopBack()  noexcept {
        const size_t cnt = 1;
        std::destroy_n(data_.GetAddress() + size_ - 1, cnt);
        --size_;
    }

    iterator Erase(const_iterator pos) {
        iterator mutable_pos = begin() + (pos - cbegin());
        std::move(mutable_pos + 1, end(), mutable_pos);
        const size_t cnt = 1;
        std::destroy_n(data_.GetAddress() + size_ - 1, cnt);
        --size_;
        return mutable_pos;
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        if (size_ == Capacity()) {
            return EmplaceInReallocatedMemory(pos, std::forward<Args>(args)...);

        }
        else {
            return EmplaceInCurrentMemory(pos, std::forward<Args>(args)...);
        }
    }

    

    iterator Insert(const_iterator pos, const T& value) {
        return Emplace(pos, value);
    }

    iterator Insert(const_iterator pos, T&& value) {
        return Emplace(pos, std::forward<T>(value));
    }

private:

    template <typename... Args>
    iterator EmplaceInReallocatedMemory(const_iterator pos, Args&&... args) {
        size_t new_size = size_ + 1;
        size_t new_item_offset = pos - cbegin();
        size_t new_capacity = size_ == 0 ? 1 : 2 * size_;
        RawMemory<T> new_data(new_capacity);
        new (new_data + new_item_offset) T(std::forward<Args>(args)...);

        //copy/move elements before insertion position
        size_t nmb_before = new_item_offset;
        try {
            RawMemory<T>::CopyOrMoveData(data_.GetAddress(),nmb_before, new_data.GetAddress());
        }
        catch (...) {
            //destroy first element
            const size_t cnt_to_destroy = 1;
            std::destroy_n(new_data.GetAddress() + new_item_offset, cnt_to_destroy);
            throw;
        }

        //copy/move elements after insertion position
        try {
            size_t nmb_after = (cend() - pos);
            RawMemory<T>::CopyOrMoveData(data_.GetAddress() + new_item_offset, nmb_after, new_data.GetAddress() + new_item_offset + 1);
        }
        catch (...) {
            //destroy already created elements
            const size_t cnt_to_destroy = new_item_offset;
            std::destroy_n(new_data.GetAddress(), cnt_to_destroy);
            throw;
        }

        //destroy useless elements in old memory and swap to new memory-data
        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);

        size_ = new_size;
        return begin() + new_item_offset;

    }

    template <typename... Args>
    iterator EmplaceInCurrentMemory(const_iterator pos, Args&&... args) {
        size_t new_size = size_ + 1;
        size_t new_item_offset = pos - cbegin();
        if (pos == cend()) {
            new (data_ + size_) T(std::forward<Args>(args)...);
        }
        else {
            T&& temp = T(std::forward<Args>(args)...);
            new (data_ + size_) T(std::move(data_[size_ - 1]));

            iterator first = begin() + new_item_offset;
            iterator last = begin() + (size_ - 1);
            std::move_backward(first, last, end());
            data_[new_item_offset] = std::move(temp);
        }
        size_ = new_size;
        return begin() + new_item_offset;
    }


    static void CopyConstruct(T* buf, const T& value) {
        new (buf) T(value);
    }


    static void Destroy(T* buf) {
        buf->~T();
    }

    RawMemory<T> data_;
    size_t size_ = 0;
};