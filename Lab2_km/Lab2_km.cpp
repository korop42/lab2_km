#include <iostream>
#include <windows.h>
#include <fstream>
#include <string>

const int NUM_THREADS = 6;
const int NUMBERS_TO_WRITE = 500;
const int MAX_CONCURRENT_THREADS = 2;
const int FILE_SIZE = 4096;

struct ThreadData {
    HANDLE hFileMapping;
    int threadId;
    int pairId;
};

CRITICAL_SECTION consoleCriticalSection; // Додано критичну секцію

DWORD WINAPI ThreadFunction(LPVOID lpParam) {
    ThreadData* data = (ThreadData*)lpParam;
    HANDLE hSemaphore = OpenSemaphore(SEMAPHORE_ALL_ACCESS, FALSE, L"MySemaphore");
    if (hSemaphore == NULL) {
        std::cerr << "Error opening semaphore: " << GetLastError() << std::endl;
        return 1;
    }
    WaitForSingleObject(hSemaphore, INFINITE);

    char* pData = (char*)MapViewOfFile(data->hFileMapping, FILE_MAP_ALL_ACCESS, 0, 0, FILE_SIZE);
    if (pData == NULL) {
        std::cerr << "Error mapping file: " << GetLastError() << std::endl;
        ReleaseSemaphore(hSemaphore, 1, NULL);
        return 1;
    }

    std::ofstream outFile("output.txt", std::ios::app);

    std::string syncMethod;
    if (data->pairId == 0) {
        syncMethod = "No Sync";
    }
    else if (data->pairId == 1) {
        syncMethod = "Event Sync";
    }
    else {
        syncMethod = "Critical Section Sync";
    }

    if (data->threadId % 2 == 1) { // Positive numbers
        for (int i = 1; i <= NUMBERS_TO_WRITE; ++i) {
            std::string s = std::to_string(i) + " ";
            outFile << s;
            memcpy(pData, s.c_str(), s.size());
            pData += s.size();

            EnterCriticalSection(&consoleCriticalSection); // Вхід в критичну секцію
            std::cout << "Pair " << data->pairId + 1 << " (" << syncMethod << "), Thread " << data->threadId << ": " << i << std::endl;
            LeaveCriticalSection(&consoleCriticalSection); // Вихід з критичної секції
        }
    }
    else { // Negative numbers
        for (int i = -1; i >= -NUMBERS_TO_WRITE; --i) {
            std::string s = std::to_string(i) + " ";
            outFile << s;
            memcpy(pData, s.c_str(), s.size());
            pData += s.size();

            EnterCriticalSection(&consoleCriticalSection); // Вхід в критичну секцію
            std::cout << "Pair " << data->pairId + 1 << " (" << syncMethod << "), Thread " << data->threadId << ": " << i << std::endl;
            LeaveCriticalSection(&consoleCriticalSection); // Вихід з критичної секції
        }
    }

    outFile.close();
    UnmapViewOfFile(pData);
    ReleaseSemaphore(hSemaphore, 1, NULL);
    CloseHandle(hSemaphore);
    return 0;
}

int main() {
    HANDLE hFile = CreateFile(L"shared.txt", GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    HANDLE hFileMapping = CreateFileMapping(hFile, NULL, PAGE_READWRITE, 0, FILE_SIZE, NULL);
    HANDLE hThreads[NUM_THREADS];
    ThreadData threadData[NUM_THREADS];
    HANDLE hSemaphore = CreateSemaphore(NULL, MAX_CONCURRENT_THREADS, MAX_CONCURRENT_THREADS, L"MySemaphore");
    HANDLE hEvents[3];
    CRITICAL_SECTION criticalSections[3];

    hEvents[0] = CreateEvent(NULL, TRUE, FALSE, NULL);
    hEvents[1] = CreateEvent(NULL, TRUE, FALSE, NULL);
    hEvents[2] = CreateEvent(NULL, TRUE, FALSE, NULL);

    InitializeCriticalSection(&criticalSections[0]);
    InitializeCriticalSection(&criticalSections[1]);
    InitializeCriticalSection(&criticalSections[2]);
    InitializeCriticalSection(&consoleCriticalSection); // Ініціалізація критичної секції для консолі

    for (int i = 0; i < NUM_THREADS; ++i) {
        threadData[i].hFileMapping = hFileMapping;
        threadData[i].threadId = i + 1;
        threadData[i].pairId = i / 2; // Assign pair ID
        hThreads[i] = CreateThread(NULL, 0, ThreadFunction, &threadData[i], CREATE_SUSPENDED, NULL);
        if (hThreads[i] == NULL) {
            std::cerr << "Error creating thread: " << GetLastError() << std::endl;
            return 1;
        }
    }

    for (int i = 0; i < NUM_THREADS; ++i) {
        ResumeThread(hThreads[i]);
    }

    WaitForMultipleObjects(NUM_THREADS, hThreads, TRUE, INFINITE);

    for (int i = 0; i < NUM_THREADS; ++i) {
        CloseHandle(hThreads[i]);
    }

    CloseHandle(hFileMapping);
    CloseHandle(hFile);
    CloseHandle(hSemaphore);

    DeleteCriticalSection(&criticalSections[0]);
    DeleteCriticalSection(&criticalSections[1]);
    DeleteCriticalSection(&criticalSections[2]);
    DeleteCriticalSection(&consoleCriticalSection); // Видалення критичної секції для консолі

    return 0;
}