#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <stdexcept>

class MarkerThread
{
public:
    MarkerThread(int id, std::vector<int>& array, std::mutex& mtx, std::condition_variable& cvStart,
        std::vector<std::condition_variable>& cvContinue, std::vector<bool>& continueSignal,
        std::vector<bool>& terminateSignal, std::atomic<bool>& startSignal)
        : id_(id), array_(array), mtx_(mtx), cvStart_(cvStart), cvContinue_(cvContinue),
        continueSignal_(continueSignal), terminateSignal_(terminateSignal), startSignal_(startSignal)
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
                int randomIndex = rand() % array_.size();
                if (array_[randomIndex] == 0)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                    array_[randomIndex] = id_;
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
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

private:
    int id_;
    std::vector<int>& array_;
    std::mutex& mtx_;
    std::condition_variable& cvStart_;
    std::vector<std::condition_variable>& cvContinue_;
    std::vector<bool>& continueSignal_;
    std::vector<bool>& terminateSignal_;
    std::atomic<bool>& startSignal_;
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
        std::vector<std::condition_variable> cvContinue(numThreads);
        std::vector<bool> continueSignal(numThreads, true);
        std::vector<bool> terminateSignal(numThreads, false);
        std::atomic<bool> startSignal(false);

        for (int i = 0; i < numThreads; ++i)
        {
            threads.emplace_back(MarkerThread(i + 1, array, mtx, cvStart, cvContinue, continueSignal, terminateSignal, startSignal));
        }

        {
            std::lock_guard<std::mutex> lock(mtx);
            startSignal.store(true);
            cvStart.notify_all();
        }

        bool allTerminated = false;
        while (!allTerminated)
        {
            for (int i = 0; i < numThreads; ++i)
            {
                std::unique_lock<std::mutex> lock(mtx);
                cvContinue[i].wait(lock, [&continueSignal, i] { return !continueSignal[i]; });
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

            if (terminateSignal[threadToTerminate - 1])
            {
                std::cerr << "Thread " << threadToTerminate << " has already terminated." << std::endl;
                continue;
            }

            terminateSignal[threadToTerminate - 1] = true;
            cvContinue[threadToTerminate - 1].notify_one();

            {
                std::unique_lock<std::mutex> lock(mtx);
                cvContinue[threadToTerminate - 1].wait(lock, [&terminateSignal, threadToTerminate] { return terminateSignal[threadToTerminate - 1]; });
            }
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
                for (int i = 0; i < numThreads; ++i)
                {
                    if (!terminateSignal[i])
                    {
                        continueSignal[i] = true;
                        cvContinue[i].notify_one();
                    }
                }
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