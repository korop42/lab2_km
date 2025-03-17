#include <windows.h>
#include <iostream>
#include <vector>
#include <strsafe.h>

using namespace std;

HANDLE hSemaphore;
HANDLE hEvent;
CRITICAL_SECTION cs;
HANDLE hFileMapping;
LPVOID lpMapAddress;
const int MAP_SIZE = 10000;
int offset = 0;

void WriteToFile(const string& msg) {
    memcpy((char*)lpMapAddress + offset, msg.c_str(), msg.size());
    offset += msg.size();
}

struct ThreadData {
    int start, end, step;
    int pairID;
    HANDLE hHeap;
};

DWORD WINAPI ThreadFunction(LPVOID lpParam) {
    ThreadData* data = (ThreadData*)lpParam;

    char* buffer = (char*)HeapAlloc(data->hHeap, 0, 100);

    WaitForSingleObject(hSemaphore, INFINITE);

    if (data->pairID == 1) {
        for (int i = data->start; i != data->end; i += data->step) {
            StringCchPrintfA(buffer, 100, "Thread %d: %d\n", data->pairID, i);
            cout << buffer;
            WriteToFile(buffer);
        }
    }
    else if (data->pairID == 2) {
        for (int i = data->start; i != data->end; i += data->step) {
            WaitForSingleObject(hEvent, INFINITE);
            StringCchPrintfA(buffer, 100, "Thread %d: %d\n", data->pairID, i);
            cout << buffer;
            WriteToFile(buffer);
            SetEvent(hEvent);
        }
    }
    else if (data->pairID == 3) {
        for (int i = data->start; i != data->end; i += data->step) {
            EnterCriticalSection(&cs);
            StringCchPrintfA(buffer, 100, "Thread %d: %d\n", data->pairID, i);
            cout << buffer;
            WriteToFile(buffer);
            LeaveCriticalSection(&cs);
        }
    }

    ReleaseSemaphore(hSemaphore, 1, NULL);
    HeapFree(data->hHeap, 0, buffer);
    return 0;
}

int main() {
    InitializeCriticalSection(&cs);
    hSemaphore = CreateSemaphore(NULL, 2, 2, NULL);
    hEvent = CreateEvent(NULL, FALSE, TRUE, NULL);

    HANDLE hFile = CreateFile(L"output.txt", GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    hFileMapping = CreateFileMapping(hFile, NULL, PAGE_READWRITE, 0, MAP_SIZE, NULL);
    lpMapAddress = MapViewOfFile(hFileMapping, FILE_MAP_ALL_ACCESS, 0, 0, MAP_SIZE);

    vector<HANDLE> threads;
    vector<ThreadData*> threadData;

    for (int pair = 1; pair <= 3; pair++) {
        HANDLE hHeap = HeapCreate(0, 0, 0);

        ThreadData* data1 = (ThreadData*)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, sizeof(ThreadData));
        data1->start = 1; data1->end = 501; data1->step = 1; data1->pairID = pair; data1->hHeap = hHeap;
        HANDLE t1 = CreateThread(NULL, 0, ThreadFunction, data1, CREATE_SUSPENDED, NULL);

        ThreadData* data2 = (ThreadData*)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, sizeof(ThreadData));
        data2->start = -1; data2->end = -501; data2->step = -1; data2->pairID = pair; data2->hHeap = hHeap;
        HANDLE t2 = CreateThread(NULL, 0, ThreadFunction, data2, CREATE_SUSPENDED, NULL);

        threads.push_back(t1);
        threads.push_back(t2);
        threadData.push_back(data1);
        threadData.push_back(data2);

        SetThreadPriority(t1, THREAD_PRIORITY_ABOVE_NORMAL);
        SetThreadPriority(t2, THREAD_PRIORITY_BELOW_NORMAL);
    }

    for (auto& t : threads) {
        ResumeThread(t);
    }

    WaitForMultipleObjects(threads.size(), threads.data(), TRUE, INFINITE);

    for (size_t i = 0; i < threads.size(); ++i) {
        CloseHandle(threads[i]);
        HeapDestroy(threadData[i]->hHeap);
    }

    UnmapViewOfFile(lpMapAddress);
    CloseHandle(hFileMapping);
    CloseHandle(hFile);
    CloseHandle(hSemaphore);
    CloseHandle(hEvent);
    DeleteCriticalSection(&cs);

    cout << "All threads completed.\n";
    return 0;
}
