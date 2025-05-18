#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <atomic>
#include "marker_thread.h"

class MarkerThreadTestFixture : public ::testing::Test {
protected:
    void SetUp() override {
        arraySize = 10;
        array.resize(arraySize, 0);
        mtx = std::make_shared<std::mutex>();
        cvStart = std::make_shared<std::condition_variable>();
        cvContinue = std::make_shared<std::vector<std::condition_variable>>(1);
        continueSignal = std::make_shared<std::vector<bool>>(1, true);
        terminateSignal = std::make_shared<std::vector<bool>>(1, false);
        startSignal = std::make_shared<std::atomic<bool>>(false);
        
        // Сохраняем оригинальную длительность пауз
        originalSleepDuration = MarkerThread::sleepDuration;
        // Уменьшаем длительность пауз для тестирования
        MarkerThread::sleepDuration = std::chrono::milliseconds(1);
    }

    void TearDown() override {
        // Восстанавливаем оригинальную длительность пауз
        MarkerThread::sleepDuration = originalSleepDuration;
    }

    int arraySize;
    std::vector<int> array;
    std::shared_ptr<std::mutex> mtx;
    std::shared_ptr<std::condition_variable> cvStart;
    std::shared_ptr<std::vector<std::condition_variable>> cvContinue;
    std::shared_ptr<std::vector<bool>> continueSignal;
    std::shared_ptr<std::vector<bool>> terminateSignal;
    std::shared_ptr<std::atomic<bool>> startSignal;
    static std::chrono::milliseconds originalSleepDuration;
};

std::chrono::milliseconds MarkerThreadTestFixture::originalSleepDuration;

TEST_F(MarkerThreadTestFixture, ThreadMarksElementsCorrectly) {
    MarkerThread thread(1, array, *mtx, *cvStart, *cvContinue, *continueSignal, *terminateSignal, *startSignal);
    
    std::thread t([&](){ thread(); });
    
    // Отправляем сигнал старт
    {
        std::lock_guard<std::mutex> lock(*mtx);
        *startSignal = true;
        cvStart->notify_all();
    }
    
    // Даем время потоку поработать
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Проверяем, что поток пометил элементы массива своим ID
    bool foundMark = false;
    for (int val : array) {
        if (val == 1) {
            foundMark = true;
            break;
        }
    }
    EXPECT_TRUE(foundMark);
    
    // Очищаем
    (*terminateSignal)[0] = true;
    (*continueSignal)[0] = true;
    cvContinue->at(0).notify_one();
    t.join();
}

TEST_F(MarkerThreadTestFixture, ThreadClearsMarksOnTermination) {
    MarkerThread thread(1, array, *mtx, *cvStart, *cvContinue, *continueSignal, *terminateSignal, *startSignal);
    
    std::thread t([&](){ thread(); });
    
    // Отправляем сигнал старт
    {
        std::lock_guard<std::mutex> lock(*mtx);
        *startSignal = true;
        cvStart->notify_all();
    }
    
    // Даем время потоку поработать
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Завершаем поток
    (*terminateSignal)[0] = true;
    (*continueSignal)[0] = true;
    cvContinue->at(0).notify_one();
    t.join();
    
    // Проверяем, что все метки потока удалены
    for (int val : array) {
        EXPECT_EQ(val, 0);
    }
}

TEST_F(MarkerThreadTestFixture, ThreadDoesNotOverwriteOtherMarks) {
    // Устанавливаем начальное значение в массиве
    array[0] = 2;
    
    MarkerThread thread(1, array, *mtx, *cvStart, *cvContinue, *continueSignal, *terminateSignal, *startSignal);
    // Устанавливаем фиксированный индекс для тестирования
    thread.setFixedIndex(0); 
    
    std::thread t([&](){ thread(); });
    
    // Отправляем сигнал старт
    {
        std::lock_guard<std::mutex> lock(*mtx);
        *startSignal = true;
        cvStart->notify_all();
    }
    
    // Даем время потоку попытаться изменить элемент
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Проверяем, что элемент не был перезаписан
    EXPECT_EQ(array[0], 2);
    
    // Очищаем
    (*terminateSignal)[0] = true;
    (*continueSignal)[0] = true;
    cvContinue->at(0).notify_one();
    t.join();
}

TEST_F(MarkerThreadTestFixture, MultipleThreadsWorkCorrectly) {
    const int numThreads = 3;
    std::vector<std::shared_ptr<std::vector<bool>>> continueSignals(numThreads);
    std::vector<std::shared_ptr<std::vector<bool>>> terminateSignals(numThreads);
    std::vector<std::shared_ptr<std::vector<std::condition_variable>>> cvContinues(numThreads);
    std::vector<std::shared_ptr<std::mutex>> mutexes(numThreads);
    
    for (int i = 0; i < numThreads; ++i) {
        continueSignals[i] = std::make_shared<std::vector<bool>>(numThreads, true);
        terminateSignals[i] = std::make_shared<std::vector<bool>>(numThreads, false);
        cvContinues[i] = std::make_shared<std::vector<std::condition_variable>>(numThreads);
        mutexes[i] = std::make_shared<std::mutex>();
    }
    
    std::vector<std::thread> threads;
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back(MarkerThread(i+1, array, *mutexes[i], *cvStart, *cvContinues[i], 
                      *continueSignals[i], *terminateSignals[i], *startSignal));
    }
    
    // Отправляем сигнал старт
    {
        std::lock_guard<std::mutex> lock(*mtx);
        *startSignal = true;
        cvStart->notify_all();
    }
    
    // Даем время потокам поработать
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Проверяем, что все элементы массива либо нули, либо имеют значения от 1 до numThreads
    for (int val : array) {
        EXPECT_TRUE(val >= 0 && val <= numThreads);
    }
    
    // Завершаем все потоки
    for (int i = 0; i < numThreads; ++i) {
        (*terminateSignals[i])[i] = true;
        (*continueSignals[i])[i] = true;
        cvContinues[i]->at(i).notify_one();
        threads[i].join();
    }
}