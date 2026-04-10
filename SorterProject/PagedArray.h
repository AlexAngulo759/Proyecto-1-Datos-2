#pragma once

#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>

using namespace std;

class PagedArray {
private:
    fstream file;
    size_t pageSize;
    size_t frameCount;
    size_t elementSize;
    size_t totalElements;
    size_t totalPages;
    size_t elemsPerPageVal;

    struct PageFrame {
        size_t pageIdx;
        char* data;
        bool dirty;
        int lruCounter;
    };

    PageFrame* frames;
    int* pageToFrame;
    int currentLRU;

    size_t elemsPerPage() const { return elemsPerPageVal; }
    size_t pageIndex(size_t idx) const { return idx / elemsPerPageVal; }
    size_t offsetInPage(size_t idx) const { return (idx % elemsPerPageVal) * elementSize; }

    size_t findLRU() {
        size_t lru = 0;
        int minCounter = frames[0].lruCounter;
        for (size_t i = 1; i < frameCount; ++i) {
            if (frames[i].lruCounter < minCounter) {
                minCounter = frames[i].lruCounter;
                lru = i;
            }
        }
        return lru;
    }

    void loadPage(size_t pageIdx) {
        int frameIdx = pageToFrame[pageIdx];
        if (frameIdx >= 0) {
            frames[frameIdx].lruCounter = ++currentLRU;
            ++totalPageHits;
            return;
        }

        ++totalPageFaults;

        size_t slot = frameCount;
        for (size_t i = 0; i < frameCount; ++i) {
            if (frames[i].data == nullptr) {
                slot = i;
                break;
            }
        }

        if (slot == frameCount) {
            slot = findLRU();
            if (frames[slot].dirty) {
                file.seekp(static_cast<streamoff>(frames[slot].pageIdx * pageSize), ios::beg);
                file.write(frames[slot].data, pageSize);
                frames[slot].dirty = false;
            }
            pageToFrame[frames[slot].pageIdx] = -1;
        }
        else {
            frames[slot].data = new char[pageSize];
        }

        frames[slot].pageIdx = pageIdx;
        frames[slot].dirty = false;
        frames[slot].lruCounter = ++currentLRU;
        pageToFrame[pageIdx] = static_cast<int>(slot);
        file.seekg(static_cast<streamoff>(pageIdx * pageSize), ios::beg);
        file.read(frames[slot].data, pageSize);
    }

    inline int getImpl(size_t idx) {
        size_t pIdx = pageIndex(idx);
        size_t off = offsetInPage(idx);
        loadPage(pIdx);
        int frameIdx = pageToFrame[pIdx];
        int value;
        memcpy(&value, frames[frameIdx].data + off, elementSize);
        return value;
    }

    inline void setImpl(size_t idx, int value) {
        size_t pIdx = pageIndex(idx);
        size_t off = offsetInPage(idx);
        loadPage(pIdx);
        int frameIdx = pageToFrame[pIdx];
        memcpy(frames[frameIdx].data + off, &value, elementSize);
        frames[frameIdx].dirty = true;
    }

public:
    static inline size_t totalPageFaults = 0;
    static inline size_t totalPageHits = 0;

    static void resetGlobalStats() {
        totalPageFaults = 0;
        totalPageHits = 0;
    }

    PagedArray(const string& filename, size_t pageSizeBytes, size_t frames, size_t elemSize = sizeof(int))
        : pageSize(pageSizeBytes), frameCount(frames), elementSize(elemSize), currentLRU(0) {
        file.open(filename, ios::in | ios::out | ios::binary);
        if (!file.is_open())
            throw runtime_error("Cannot open file");
        file.seekg(0, ios::end);
        totalElements = static_cast<size_t>(file.tellg()) / elementSize;
        elemsPerPageVal = pageSize / elementSize;
        totalPages = (totalElements + elemsPerPageVal - 1) / elemsPerPageVal;

        this->frames = new PageFrame[frameCount];
        for (size_t i = 0; i < frameCount; ++i) {
            this->frames[i].pageIdx = 0;
            this->frames[i].data = nullptr;
            this->frames[i].dirty = false;
            this->frames[i].lruCounter = 0;
        }

        pageToFrame = new int[totalPages];
        for (size_t i = 0; i < totalPages; ++i) {
            pageToFrame[i] = -1;
        }
    }

    ~PagedArray() {
        flushAll();
        file.close();
        for (size_t i = 0; i < frameCount; ++i) {
            delete[] frames[i].data;
        }
        delete[] frames;
        delete[] pageToFrame;
    }

    size_t size() const { return totalElements; }

    void flushAll() {
        for (size_t i = 0; i < frameCount; ++i) {
            if (frames[i].dirty && frames[i].data) {
                file.seekp(static_cast<streamoff>(frames[i].pageIdx * pageSize), ios::beg);
                file.write(frames[i].data, pageSize);
                frames[i].dirty = false;
            }
        }
    }

    class Proxy {
    private:
        PagedArray& arr;
        size_t idx;
    public:
        Proxy(PagedArray& a, size_t i) : arr(a), idx(i) {}
        inline operator int() const { return arr.getImpl(idx); }
        inline Proxy& operator=(int val) { arr.setImpl(idx, val); return *this; }
        inline Proxy& operator=(const Proxy& other) {
            arr.setImpl(idx, other.arr.getImpl(other.idx));
            return *this;
        }
        friend inline void swap(Proxy& a, Proxy& b) {
            int temp = a;
            a = b;
            b = temp;
        }
    };

    inline Proxy operator[](size_t idx) { return Proxy(*this, idx); }
    inline int operator[](size_t idx) const { return const_cast<PagedArray*>(this)->getImpl(idx); }
};