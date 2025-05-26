#define BOOST_TEST_MODULE MarkerThreadTest
#include <boost/test/unit_test.hpp>
#include <thread>
#include <chrono>
#include "marker_thread.h"

class MarkerThreadTestFixture {
public:
    MarkerThreadTestFixture() {
        arraySize = 10;
        array.resize(arraySize, 0);
        mtx = std::make_shared<std::mutex>();
        cvStart = std::make_shared<std::condition_variable>();
        cvContinue = std::make_shared<std::vector<std::condition_variable>>(1);
        continueSignal = std::make_shared<std::vector<bool>>(1, true);
        terminateSignal = std::make_shared<std::vector<bool>>(1, false);
        startSignal = std::make_shared<std::atomic<bool>>(false);
        
        originalSleepDuration = MarkerThread::sleepDuration;
        MarkerThread::sleepDuration = std::chrono::milliseconds(1);
    }

    ~MarkerThreadTestFixture() {
        MarkerThread::sleepDuration = originalSleepDuration;
    }

    static std::chrono::milliseconds originalSleepDuration;

    int arraySize;
    std::vector<int> array;
    std::shared_ptr<std::mutex> mtx;
    std::shared_ptr<std::condition_variable> cvStart;
    std::shared_ptr<std::vector<std::condition_variable>> cvContinue;
    std::shared_ptr<std::vector<bool>> continueSignal;
    std::shared_ptr<std::vector<bool>> terminateSignal;
    std::shared_ptr<std::atomic<bool>> startSignal;
};

std::chrono::milliseconds MarkerThreadTestFixture::originalSleepDuration;

BOOST_FIXTURE_TEST_CASE(ThreadStartsAfterSignal, MarkerThreadTestFixture) {
    MarkerThread thread(1, array, *mtx, *cvStart, *cvContinue, *continueSignal, *terminateSignal, *startSignal);
    
    std::atomic<bool> threadStarted(false);
    std::thread t([&](){
        threadStarted = true;
        thread();
    });
    
    BOOST_CHECK(!threadStarted.load());
    
    {
        std::lock_guard<std::mutex> lock(*mtx);
        *startSignal = true;
        cvStart->notify_all();
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    BOOST_CHECK(threadStarted.load());
    
    (*terminateSignal)[0] = true;
    (*continueSignal)[0] = true;
    cvContinue->at(0).notify_one();
    t.join();
}

BOOST_FIXTURE_TEST_CASE(ThreadMarksElementsCorrectly, MarkerThreadTestFixture) {
    MarkerThread thread(1, array, *mtx, *cvStart, *cvContinue, *continueSignal, *terminateSignal, *startSignal);
    
    std::thread t([&](){ thread(); });
    
    {
        std::lock_guard<std::mutex> lock(*mtx);
        *startSignal = true;
        cvStart->notify_all();
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    bool foundMark = false;
    for (int val : array) {
        if (val == 1) {
            foundMark = true;
            break;
        }
    }
    BOOST_CHECK(foundMark);
    
    (*terminateSignal)[0] = true;
    (*continueSignal)[0] = true;
    cvContinue->at(0).notify_one();
    t.join();
}

BOOST_FIXTURE_TEST_CASE(ThreadClearsMarksOnTermination, MarkerThreadTestFixture) {
    MarkerThread thread(1, array, *mtx, *cvStart, *cvContinue, *continueSignal, *terminateSignal, *startSignal);
    
    std::thread t([&](){ thread(); });
    
    {
        std::lock_guard<std::mutex> lock(*mtx);
        *startSignal = true;
        cvStart->notify_all();
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    (*terminateSignal)[0] = true;
    (*continueSignal)[0] = true;
    cvContinue->at(0).notify_one();
    t.join();
    
    for (int val : array) {
        BOOST_CHECK_EQUAL(val, 0);
    }
}

BOOST_FIXTURE_TEST_CASE(ThreadDoesNotOverwriteOtherMarks, MarkerThreadTestFixture) {
    array[0] = 2;
    
    MarkerThread thread(1, array, *mtx, *cvStart, *cvContinue, *continueSignal, *terminateSignal, *startSignal);
    thread.setFixedIndex(0); 
    
    std::thread t([&](){ thread(); });
    
    {
        std::lock_guard<std::mutex> lock(*mtx);
        *startSignal = true;
        cvStart->notify_all();
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    BOOST_CHECK_EQUAL(array[0], 2);
    
    (*terminateSignal)[0] = true;
    (*continueSignal)[0] = true;
    cvContinue->at(0).notify_one();
    t.join();
}

BOOST_FIXTURE_TEST_CASE(MultipleThreadsWorkCorrectly, MarkerThreadTestFixture) {
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
    
    {
        std::lock_guard<std::mutex> lock(*mtx);
        *startSignal = true;
        cvStart->notify_all();
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    for (int val : array) {
        BOOST_CHECK(val >= 0 && val <= numThreads);
    }
    
    for (int i = 0; i < numThreads; ++i) {
        (*terminateSignals[i])[i] = true;
        (*continueSignals[i])[i] = true;
        cvContinues[i]->at(i).notify_one();
        threads[i].join();
    }
}