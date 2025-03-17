#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <stdexcept>

class MarkerThread {
public:
    MarkerThread(int id, std::vector<int>& array, std::mutex& mtx, std::condition_variable& cvStart, std::condition_variable& cvContinue,
        std::atomic<bool>& startSignal, std::atomic<bool>& continueSignal, std::atomic<bool>& terminateSignal)
        : id_(id), array_(array), mtx_(mtx), cvStart_(cvStart), cvContinue_(cvContinue),
        startSignal_(startSignal), continueSignal_(continueSignal), terminateSignal_(terminateSignal) {
    }

    void operator()()
    {
        try
        {
            std::unique_lock<std::mutex> lock(mtx_);
            cvStart_.wait(lock, [this] { return startSignal_.load(); });

            srand(id_);

            int markedCount = 0;
            while (!terminateSignal_.load())
            {
                int randomIndex = rand() % array_.size();
                if (array_[randomIndex] == 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                    array_[randomIndex] = id_;
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                    ++markedCount;
                }
                else
                {
                    std::cout << "Thread " << id_ << ": marked " << markedCount << " elements, cannot mark index " << randomIndex << std::endl;
                    continueSignal_.store(false);
                    cvContinue_.notify_one();
                    cvContinue_.wait(lock, [this] { return continueSignal_.load() || terminateSignal_.load(); });
                    if (terminateSignal_.load())
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
        }
        catch (const std::exception& e)
        {
            std::cerr << "Exception in thread " << id_ << ": " << e.what() << std::endl;
        }
    }

private:
    int id_;
    std::vector<int>& array_;
    std::mutex& mtx_;
    std::condition_variable& cvStart_;
    std::condition_variable& cvContinue_;
    std::atomic<bool>& startSignal_;
    std::atomic<bool>& continueSignal_;
    std::atomic<bool>& terminateSignal_;
};

void printArray(const std::vector<int>& array)
{
    for (int num : array) {
        std::cout << num << " ";
    }
    std::cout << std::endl;
}

int main()
{
    try
    {
        int arraySize;
        std::cout << "Enter the size of the array: ";
        std::cin >> arraySize;

        if (arraySize <= 0)
        {
            throw std::invalid_argument("Array size must be positive.");
        }

        std::vector<int> array(arraySize, 0);

        int numThreads;
        std::cout << "Enter the number of marker threads: ";
        std::cin >> numThreads;

        if (numThreads <= 0)
        {
            throw std::invalid_argument("Number of threads must be positive.");
        }

        std::vector<std::thread> threads;
        std::mutex mtx;
        std::condition_variable cvStart;
        std::condition_variable cvContinue;
        std::atomic<bool> startSignal(false);
        std::atomic<bool> continueSignal(true);
        std::atomic<bool> terminateSignal(false);

        for (int i = 0; i < numThreads; ++i)
        {
            threads.emplace_back(MarkerThread(i + 1, array, mtx, cvStart, cvContinue, startSignal, continueSignal, terminateSignal));
        }

        {
            std::lock_guard<std::mutex> lock(mtx);
            startSignal.store(true);
            cvStart.notify_all();
        }

        bool allTerminated = false;
        while (!allTerminated)
        {
            {
                std::unique_lock<std::mutex> lock(mtx);
                cvContinue.wait(lock, [&continueSignal] { return !continueSignal.load(); });
            }

            printArray(array);

            int threadToTerminate;
            std::cout << "Enter the number of the thread to terminate: ";
            std::cin >> threadToTerminate;

            if (threadToTerminate < 1 || threadToTerminate > numThreads)
            {
                std::cerr << "Invalid thread number." << std::endl;
                continue;
            }

            terminateSignal.store(true);
            cvContinue.notify_one();

            threads[threadToTerminate - 1].join();

            printArray(array);

            allTerminated = true;
            for (const auto& t : threads)
            {
                if (t.joinable())
                {
                    allTerminated = false;
                    break;
                }
            }

            if (!allTerminated)
            {
                terminateSignal.store(false);
                continueSignal.store(true);
                cvContinue.notify_all();
            }
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception in main: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}