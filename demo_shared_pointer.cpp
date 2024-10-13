#include <iostream>

template<typename T>

class MySharedPointer {
    private:
        T* m_ptr;
        int* m_refCount;

    public: 
    // constructor
    // default T = null, if T != null => m_refCount increases
    explicit MySharedPointer(T* ptr = nullptr) : m_ptr(ptr), m_refCount(new int(1)) {
        std::cout << "Tạo tự động" << std::endl;
        if (!ptr) {
            *m_refCount = 0;
        }
    }

    // copy constructor
    MySharedPointer(const MySharedPointer& other) {
        m_ptr = other.m_ptr;
        m_refCount = other.m_refCount;
        if (m_refCount) {
            (*m_refCount)++;
        }
    }

    // destructor
    ~MySharedPointer() {
        if (m_refCount && --(*m_refCount) == 0) {
            delete m_refCount;
            delete m_ptr;
            std::cout << "Đã giải phóng bộ nhớ!" << std::endl;
        }
    }

    // assignment operator
    MySharedPointer& operator=(const MySharedPointer& other) {
        if (this != &other) {
            if (m_refCount && --(*m_refCount) == 0) {
                delete m_refCount;
                delete m_ptr;
            }
            m_ptr = other.m_ptr;
            m_refCount = other.m_refCount;
            if (m_refCount) {
                (*m_refCount)++;
            }
        }
        return *this;
    }

    // dereference
    T& operator*() const {
        return *m_ptr;
    }

    T* operator->() const {
        return m_ptr;
    }

    int getCount() const {
        return m_refCount? *m_refCount : 0;
    }
};

class SinhVien {
    public: 
    std::string ten;
    SinhVien(const std::string &name) : ten(name) {}
    ~SinhVien() {}

    void print() {
        std::cout << "Sinh vien: " << ten << std::endl;
    }
};

int main () {
    MySharedPointer<SinhVien> sp1(new SinhVien("Nguyen Van A"));
    std::cout << "Số lượng con trỏ đang trỏ tới: " << sp1.getCount() << std::endl;

    {
        MySharedPointer<SinhVien> sp2 = sp1;  // Copy constructor
        std::cout << "Số lượng con trỏ đang trỏ tới: " << sp1.getCount() << std::endl;

        sp2->print();
    }

    std::cout << "Sau khi sp2 ra khỏi phạm vi, số lượng con trỏ còn lại: " << sp1.getCount() << std::endl;

    return 0;
}

