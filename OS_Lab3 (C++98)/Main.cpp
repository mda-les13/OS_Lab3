#include <windows.h>
#include <iostream>
#include <vector>

using namespace std;

class MarkerThread
{
public:
    MarkerThread(int id, std::vector<int>& array, CRITICAL_SECTION& mtx, HANDLE cvStart,
                 std::vector<HANDLE>& cvContinue, std::vector<bool>& continueSignal,
                 std::vector<bool>& terminateSignal, bool& startSignal)
        : id_(id), array_(array), mtx_(mtx), cvStart_(cvStart), cvContinue_(cvContinue),
          continueSignal_(continueSignal), terminateSignal_(terminateSignal), startSignal_(startSignal)
    {
    }

    void operator()()
    {
        try
        {
            // Wait for start signal
            EnterCriticalSection(&mtx_);
            while (!startSignal_)
            {
                LeaveCriticalSection(&mtx_);
                WaitForSingleObject(cvStart_, INFINITE);
                EnterCriticalSection(&mtx_);
            }
            LeaveCriticalSection(&mtx_);

            srand(id_);

            int markedCount = 0;
            while (true)
            {
                EnterCriticalSection(&mtx_);
                if (terminateSignal_[id_ - 1])
                {
                    LeaveCriticalSection(&mtx_);
                    break;
                }

                int randomIndex = rand() % array_.size();
                if (array_[randomIndex] == 0)
                {
                    Sleep(5);
                    array_[randomIndex] = id_;
                    Sleep(5);
                    ++markedCount;
                    LeaveCriticalSection(&mtx_);
                }
                else
                {
                    cout << "Thread " << id_ << ": marked " << markedCount << " elements, cannot mark index " << randomIndex << endl;
                    continueSignal_[id_ - 1] = false;
                    SetEvent(cvContinue_[id_ - 1]);
                    LeaveCriticalSection(&mtx_);

                    EnterCriticalSection(&mtx_);
                    while (!continueSignal_[id_ - 1] && !terminateSignal_[id_ - 1])
                    {
                        LeaveCriticalSection(&mtx_);
                        WaitForSingleObject(cvContinue_[id_ - 1], INFINITE);
                        EnterCriticalSection(&mtx_);
                    }

                    if (terminateSignal_[id_ - 1])
                    {
                        LeaveCriticalSection(&mtx_);
                        break;
                    }

                    continueSignal_[id_ - 1] = false;
                    LeaveCriticalSection(&mtx_);
                }
            }

            // Clear own marks
            EnterCriticalSection(&mtx_);
            for (size_t i = 0; i < array_.size(); ++i)
            {
                if (array_[i] == id_)
                {
                    array_[i] = 0;
                }
            }
            terminateSignal_[id_ - 1] = true;
            LeaveCriticalSection(&mtx_);
            SetEvent(cvContinue_[id_ - 1]);
        }
        catch (...)
        {
            cerr << "Exception in thread " << id_ << endl;
        }
    }

private:
    int id_;
    std::vector<int>& array_;
    CRITICAL_SECTION& mtx_;
    HANDLE cvStart_;
    std::vector<HANDLE>& cvContinue_;
    std::vector<bool>& continueSignal_;
    std::vector<bool>& terminateSignal_;
    bool& startSignal_;
};

DWORD WINAPI ThreadProc(LPVOID lpParam)
{
    MarkerThread* pMT = (MarkerThread*)lpParam;
    (*pMT)();
    return 0;
}

void printArray(const std::vector<int>& array)
{
    for (size_t i = 0; i < array.size(); ++i)
    {
        cout << array[i] << " ";
    }
    cout << endl;
}

int main()
{
    int arraySize;
    cout << "Enter the size of the array: ";
    cin >> arraySize;

    if (arraySize <= 0)
    {
        cerr << "Array size must be positive." << endl;
        return 1;
    }

    std::vector<int> array(arraySize, 0);

    int numThreads;
    cout << "Enter the number of marker threads: ";
    cin >> numThreads;

    if (numThreads <= 0)
    {
        cerr << "Number of threads must be positive." << endl;
        return 1;
    }

    // Synchronization primitives
    CRITICAL_SECTION mtx;
    InitializeCriticalSection(&mtx);
    HANDLE cvStartEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    std::vector<HANDLE> cvContinueEvents(numThreads);
    for (int i = 0; i < numThreads; ++i)
    {
        cvContinueEvents[i] = CreateEvent(NULL, FALSE, FALSE, NULL);
    }

    std::vector<bool> continueSignal(numThreads, true);
    std::vector<bool> terminateSignal(numThreads, false);
    bool startSignal = false;

    std::vector<MarkerThread*> markerThreads;
    std::vector<HANDLE> threadHandles;

    for (int i = 0; i < numThreads; ++i)
    {
        MarkerThread* pMT = new MarkerThread(i + 1, array, mtx, cvStartEvent,
                                             cvContinueEvents, continueSignal,
                                             terminateSignal, startSignal);
        markerThreads.push_back(pMT);

        HANDLE hThread = CreateThread(NULL, 0, ThreadProc, pMT, 0, NULL);
        if (!hThread)
        {
            cerr << "Failed to create thread " << i + 1 << endl;
            delete pMT;
            return 1;
        }
        threadHandles.push_back(hThread);
    }

    {
        EnterCriticalSection(&mtx);
        startSignal = true;
        LeaveCriticalSection(&mtx);
        SetEvent(cvStartEvent);
    }

    bool allTerminated = false;
    while (!allTerminated)
    {
        for (int i = 0; i < numThreads; ++i)
        {
            while (true)
            {
                EnterCriticalSection(&mtx);
                if (!continueSignal[i])
                {
                    LeaveCriticalSection(&mtx);
                    break;
                }
                LeaveCriticalSection(&mtx);
                Sleep(100);
            }
        }

        printArray(array);

        int threadToTerminate;
        cout << "Enter the number of the thread to terminate: ";
        cin >> threadToTerminate;

        if (threadToTerminate < 1 || threadToTerminate > numThreads)
        {
            cerr << "Invalid thread number." << endl;
            continue;
        }

        int idx = threadToTerminate - 1;
        EnterCriticalSection(&mtx);
        if (terminateSignal[idx])
        {
            cerr << "Thread " << threadToTerminate << " has already terminated." << endl;
            LeaveCriticalSection(&mtx);
            continue;
        }

        terminateSignal[idx] = true;
        LeaveCriticalSection(&mtx);
        SetEvent(cvContinueEvents[idx]);

        WaitForSingleObject(threadHandles[idx], INFINITE);
        CloseHandle(threadHandles[idx]);
        threadHandles[idx] = NULL;
        delete markerThreads[idx];

        printArray(array);

        allTerminated = true;
        for (int i = 0; i < numThreads; ++i)
        {
            if (threadHandles[i] != NULL && !terminateSignal[i])
            {
                allTerminated = false;
                break;
            }
        }

        if (!allTerminated)
        {
            EnterCriticalSection(&mtx);
            for (int i = 0; i < numThreads; ++i)
            {
                if (!terminateSignal[i])
                {
                    continueSignal[i] = true;
                    SetEvent(cvContinueEvents[i]);
                }
            }
            LeaveCriticalSection(&mtx);
        }
    }

    // Cleanup
    CloseHandle(cvStartEvent);
    for (int i = 0; i < numThreads; ++i)
    {
        CloseHandle(cvContinueEvents[i]);
    }
    DeleteCriticalSection(&mtx);

    return 0;
}