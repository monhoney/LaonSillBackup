/**
 * @file MemoryMgmt.cpp
 * @date 2017-12-05
 * @author moonhoen lee
 * @brief 
 * @details
 */

#include <sys/time.h>
#include <time.h>

#include <vector>
#include <algorithm>

#include "MemoryMgmt.h"
#include "SysLog.h"
#include "Param.h"
#include "FileMgmt.h"

using namespace std;

extern const char*  LAONSILL_HOME_ENVNAME;

map<void*, MemoryEntry>     MemoryMgmt::entryMap;
mutex                       MemoryMgmt::entryMutex;
uint64_t                    MemoryMgmt::usedMemTotalSize;
uint64_t                    MemoryMgmt::currIndex;
uint64_t                    MemoryMgmt::usedGPUMemTotalSize;

void MemoryMgmt::init() {
    char dumpDir[PATH_MAX];
    SASSERT0(sprintf(dumpDir, "%s/dump", getenv(LAONSILL_HOME_ENVNAME)) != -1);
    FileMgmt::checkDir(dumpDir);

    MemoryMgmt::usedMemTotalSize = 0ULL;
    MemoryMgmt::currIndex = 0ULL;

    MemoryMgmt::usedGPUMemTotalSize = 0ULL;
}

void MemoryMgmt::insertEntry(const char* filename, const char* funcname, int line,
        unsigned long size, bool once, void* ptr, bool cpu) {
    MemoryEntry entry;
    strcpy(entry.filename, filename);
    strcpy(entry.funcname, funcname);
    entry.line = line;
    entry.size = size;
    entry.once = once;
    entry.cpu = cpu;

    unique_lock<mutex> entryMutexLock(MemoryMgmt::entryMutex);
    entry.index = MemoryMgmt::currIndex;
    MemoryMgmt::currIndex = MemoryMgmt::currIndex + 1;

    // XXX: LMH debug
    if (MemoryMgmt::entryMap.find(ptr) != MemoryMgmt::entryMap.end()) {
        entryMutexLock.unlock();
        dump(MemoryMgmtSortOptionNone, true);

        if (cpu) {
            printf("[cpu] filename : %s, funcname : %s, line : %d, size : %lu, pointer : %p\n", 
                    filename, funcname, line, size, ptr);
        } else {
            printf("[gpu] filename : %s, funcname : %s, line : %d, size : %lu, pointer : %p\n", 
                    filename, funcname, line, size, ptr);
        }
        entryMutexLock.lock();
    }

    SASSUME0(MemoryMgmt::entryMap.find(ptr) == MemoryMgmt::entryMap.end());
    MemoryMgmt::entryMap[ptr] = entry;

    if (cpu) {
        // XXX: CPU ???????????? page size?????? ?????????????????? ??? ?????? ????????? GPU??? ????????? ????????????
        //      ????????? ??????, page size??? ?????? ????????? ?????? ????????? size????????? ??????????????? ??????.
        //      ????????? ????????? ?????? ????????? ????????? ?????? :)
        MemoryMgmt::usedMemTotalSize += size;
    } else {
        MemoryMgmt::usedGPUMemTotalSize += ALIGNUP(size, SPARAM(CUDA_MEMPAGE_SIZE));
    }
}

void MemoryMgmt::removeEntry(void* ptr) {
    map<void*, MemoryEntry>::iterator it;
    unique_lock<mutex> entryMutexLock(MemoryMgmt::entryMutex);    
    it = MemoryMgmt::entryMap.find(ptr);
    SASSUME0(it != MemoryMgmt::entryMap.end());

    if (MemoryMgmt::entryMap[ptr].cpu) {
        MemoryMgmt::usedMemTotalSize -= MemoryMgmt::entryMap[ptr].size;
    } else {
        MemoryMgmt::usedGPUMemTotalSize -= 
            ALIGNUP(MemoryMgmt::entryMap[ptr].size, SPARAM(CUDA_MEMPAGE_SIZE));
    }
    MemoryMgmt::entryMap.erase(it);
}

// WARNING: ??? ????????? ?????? ????????? ????????????. ????????? ?????? ???????????? ?????????.
void MemoryMgmt::dump(MemoryMgmtSortOption option, bool skipOnce) {
    FILE*           fp;
    char            dumpFilePath[PATH_MAX];
    struct timeval  val;
    struct tm*      tmPtr;

    gettimeofday(&val, NULL);
    tmPtr = localtime(&val.tv_sec);

    SASSERT0(sprintf(dumpFilePath, "%s/dump/%04d%02d%02d_%02d%02d%02d_%06ld.dump",
        getenv(LAONSILL_HOME_ENVNAME), tmPtr->tm_year + 1900, tmPtr->tm_mon + 1,
        tmPtr->tm_mday, tmPtr->tm_hour, tmPtr->tm_min, tmPtr->tm_sec, val.tv_usec) != -1);

    fp = fopen(dumpFilePath, "w+");
    SASSERT0(fp != NULL);

    if (option == MemoryMgmtSortOptionNone) {
        fprintf(fp, "no option, ");
    } else if (option == MemoryMgmtSortOptionIndex) {
        fprintf(fp, "index sort, ");
    } else {
        SASSERT0(option == MemoryMgmtSortOptionSize);
        fprintf(fp, "size sort, ");
    }

    if (skipOnce) {
        fprintf(fp, "skip once.\n");
    } else {
        fprintf(fp, "print all.\n");
    }
    fflush(fp);

    uint64_t cpuUsedMemSize = 0ULL;
    uint64_t gpuUsedMemSize = 0ULL;

    map<void*, MemoryEntry>::iterator it;
    unique_lock<mutex> entryMutexLock(MemoryMgmt::entryMutex);    
    map<void*, MemoryEntry> cp = MemoryMgmt::entryMap;
    cpuUsedMemSize = MemoryMgmt::usedMemTotalSize; 
    gpuUsedMemSize = MemoryMgmt::usedGPUMemTotalSize;
    entryMutexLock.unlock();

    fprintf(fp, "Used CPU Memory size : %f GB(%llu byte)\n",
            (float)cpuUsedMemSize / (1024.0 * 1024.0 * 1024.0), cpuUsedMemSize);
    fprintf(fp, "Used GPU Memory size : %f GB(%llu byte)\n",
            (float)gpuUsedMemSize / (1024.0 * 1024.0 * 1024.0), gpuUsedMemSize);

    if (option == MemoryMgmtSortOptionNone) {
        for (it = cp.begin(); it != cp.end(); it++) {
            void* ptr = it->first;
            MemoryEntry entry = it->second;

            if (skipOnce && entry.once)
                continue;

            if (entry.cpu) {
                SASSUME0(fprintf(fp, "[%p|%llu] : %llu cpu (%s()@%s:%d\n", ptr, entry.index, 
                    entry.size, entry.funcname, entry.filename, entry.line) > 0);
            } else {
                SASSUME0(fprintf(fp, "[%p|%llu] : %llu gpu (%s()@%s:%d\n", ptr, entry.index, 
                    entry.size, entry.funcname, entry.filename, entry.line) > 0);
            }
        }
    } else if (option == MemoryMgmtSortOptionIndex) {
        vector<pair<void*, uint64_t>> vec;

        // XXX: ??? ?????? ????????? ????????????.. ????????? vector??? ????????? ????????? ???????????? ????????????.
        //      dump() ?????? ?????? ????????? ??? ????????? ?????? ??? ??????. 
        //      ???????????? deep learing framework?????? ??? ????????? ??? ????????? ?????? ??? ??????.
        for (it = cp.begin(); it != cp.end(); it++)
            vec.push_back(make_pair(it->first, it->second.index));

        struct memoryMgmtSortIndexPred {
            bool operator()(
                const pair<void*, uint64_t> &left, const pair<void*, uint64_t> &right ) {
                return left.second < right.second;
            }
        };
        sort(vec.begin(), vec.end(), memoryMgmtSortIndexPred());

        for (int i = 0; i < vec.size(); i++) {
            void* ptr = vec[i].first;
            MemoryEntry entry = cp[ptr];

            if (skipOnce && entry.once)
                continue;

            if (entry.cpu) {
                SASSUME0(fprintf(fp, "[%p|%llu] : %llu cpu (%s()@%s:%d\n", ptr, entry.index, 
                    entry.size, entry.funcname, entry.filename, entry.line) > 0);
            } else {
                SASSUME0(fprintf(fp, "[%p|%llu] : %llu gpu (%s()@%s:%d\n", ptr, entry.index, 
                    entry.size, entry.funcname, entry.filename, entry.line) > 0);
            }
        }

    } else { 
        SASSERT0(option == MemoryMgmtSortOptionSize);
        vector<pair<void*, unsigned long>> vec;

        for (it = cp.begin(); it != cp.end(); it++)
            vec.push_back(make_pair(it->first, it->second.size));

        struct memoryMgmtSortIndexSize {
            bool operator()(
                const pair<void*, unsigned long> &left, 
                const pair<void*, unsigned long> &right ) {
                return left.second > right.second;
            }
        };
        sort(vec.begin(), vec.end(), memoryMgmtSortIndexSize());

        for (int i = 0; i < vec.size(); i++) {
            void* ptr = vec[i].first;
            MemoryEntry entry = cp[ptr];

            if (skipOnce && entry.once)
                continue;

            if (entry.cpu) {
                SASSUME0(fprintf(fp, "[%p|%llu] : %llu cpu (%s()@%s:%d\n", ptr, entry.index, 
                    entry.size, entry.funcname, entry.filename, entry.line) > 0);
            } else {
                SASSUME0(fprintf(fp, "[%p|%llu] : %llu gpu (%s()@%s:%d\n", ptr, entry.index, 
                    entry.size, entry.funcname, entry.filename, entry.line) > 0);
            }
        }
    }

    fflush(fp);
    fclose(fp);
}
