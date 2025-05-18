// marker_thread.h
#pragma once
#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>

class MarkerThread
{
public:
    // Добавляем статическую переменную для управления паузами в тестах
    static std::chrono::milliseconds sleepDuration;

    MarkerThread(int id, std::vector<int>& array, std::mutex& mtx, 
                std::condition_variable& cvStart, std::vector<std::condition_variable>& cvContinue, 
                std::vector<bool>& continueSignal, std::vector<bool>& terminateSignal, 
                std::atomic<bool>& startSignal)
        : id_(id), array_(array), mtx_(mtx), cvStart_(cvStart), cvContinue_(cvContinue),
        continueSignal_(continueSignal), terminateSignal_(terminateSignal), startSignal_(startSignal),
        fixedIndex_(-1), useFixedIndex_(false)
    {
    }

    void operator()()
    {
        try
        {
            std::unique_lock<std::mutex> lock(mtx_);
            cvStart_.wait(lock, [this] { return startSignal_.load(); });

            srand(id_);

            int markedCount = 0;
            while (!terminateSignal_[id_ - 1])
            {
                int randomIndex;
                if (useFixedIndex_) {
                    randomIndex = fixedIndex_;
                } else {
                    randomIndex = rand() % array_.size();
                }
                
                if (array_[randomIndex] == 0)
                {
                    std::this_thread::sleep_for(sleepDuration);
                    array_[randomIndex] = id_;
                    std::this_thread::sleep_for(sleepDuration);
                    ++markedCount;
                }
                else
                {
                    std::cout << "Thread " << id_ << ": marked " << markedCount << " elements, cannot mark index " << randomIndex << std::endl;
                    continueSignal_[id_ - 1] = false;
                    cvContinue_[id_ - 1].notify_one();

                    cvContinue_[id_ - 1].wait(lock, [this] { return continueSignal_[id_ - 1] || terminateSignal_[id_ - 1]; });
                    if (terminateSignal_[id_ - 1])
                    {
                        break;
                    }
                }
            }

            for (size_t i = 0; i < array_.size(); ++i)
            {
                if (array_[i] == id_)
                {
                    array_[i] = 0;
                }
            }

            terminateSignal_[id_ - 1] = true;
            cvContinue_[id_ - 1].notify_one();
        }
        catch (const std::exception& e)
        {
            std::cerr << "Exception in thread " << id_ << ": " << e.what() << std::endl;
        }
    }

    // Методы для тестирования
    void setFixedIndex(int index) {
        fixedIndex_ = index;
        useFixedIndex_ = true;
    }

private:
    int id_;
    std::vector<int>& array_;
    std::mutex& mtx_;
    std::condition_variable& cvStart_;
    std::vector<std::condition_variable>& cvContinue_;
    std::vector<bool>& continueSignal_;
    std::vector<bool>& terminateSignal_;
    std::atomic<bool>& startSignal_;
    int fixedIndex_;
    bool useFixedIndex_;
};

// Инициализация статической переменной
std::chrono::milliseconds MarkerThread::sleepDuration = std::chrono::milliseconds(5);