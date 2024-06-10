// -----------------------------------------------------------------------------
// Copyright (C) 2017  Ludovic LE FRIOUX
//
// PaInleSS is free software: you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any later
// version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
// details.
//
// You should have received a copy of the GNU General Public License along with
// this program.  If not, see <http://www.gnu.org/licenses/>.
// -----------------------------------------------------------------------------

#include <sys/resource.h>
#include "utils/System.h"
#include "utils/Logger.h"
#include "utils/ErrorCodes.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <limits>
#include <unistd.h>
#include <stdlib.h>
#include <sys/resource.h>

#ifndef __GLIBC__
#error "This code requires GNU C Library (glibc) for getUsedMemory()"
#endif

#include <malloc.h>
#include <climits>

// Memory
// ======

MemInfo *MemInfo::singleton_ = nullptr;

MemInfo::MemInfo() : owner(std::this_thread::get_id())
{
    this->parseMemInfo();
}

bool MemInfo::testOwner()
{
    if (std::this_thread::get_id() != this->owner)
    {
        LOGWARN("Thread %lu is trying to access MemInfo owned by %lu.", std::this_thread::get_id(), this->owner.load());
        return false;
    }
    else
    {
        return true;
    }
}

MemInfo *MemInfo::getInstance()
{
    if (singleton_ == nullptr)
    {
        singleton_ = new MemInfo();
    }

    if (singleton_->testOwner())
    {
        return singleton_;
    }
    else
    {
        LOGERROR("Please access MemInfo via a single thread, otherwise the data doesn't make sense. Use deleteInstance() to change owner");
        exit(PERR_NOT_SUPPORTED);
    }
}

void MemInfo::deleteInstance()
{
    MemInfo *meminfo = getInstance();
    if (meminfo != nullptr)
        delete meminfo;
    singleton_ = nullptr;
}

void MemInfo::parseMemInfo()
{

    std::ifstream meminfo_file("/proc/meminfo");
    if (!meminfo_file.is_open())
    {
        this->MemTotal = std::numeric_limits<long>::max();
        this->MemFree = std::numeric_limits<long>::max();
        this->MemAvailable = std::numeric_limits<long>::max();
        return;
    }

    std::string line;
    while (std::getline(meminfo_file, line))
    {
        std::istringstream iss(line);
        std::string key;
        long value;

        if (iss >> key >> value)
        {
            if (key == "MemTotal:")
            {
                this->MemTotal = value;
                LOGDEBUG2("Parsed MemTotal: %ld", this->MemTotal);
            }
            else if (key == "MemFree:")
            {
                this->MemFree = value;
                LOGDEBUG2("Parsed MemFree: %ld", this->MemFree);
            }
            else if (key == "MemAvailable:")
            {
                this->MemAvailable = value;
                LOGDEBUG2("Parsed MemAvailable: %ld", this->MemAvailable);
            }
        }
    }

    meminfo_file.close();
}

#if defined(__GLIBC__) && (__GLIBC__ >= 2 && __GLIBC_MINOR__ >= 33)

double MemInfo::getUsedMemory()
{
    // struct rusage r_usage;
    // getrusage(RUSAGE_SELF, &r_usage);
    // return r_usage.ru_maxrss;
    struct mallinfo2 memInfoMalloc = mallinfo2();
    LOGDEBUG1("Total non-mmapped bytes (arena):       %llu", memInfoMalloc.arena);
    LOGDEBUG1("# of free chunks (ordblks):            %llu", memInfoMalloc.ordblks);
    LOGDEBUG1("# of free fastbin blocks (smblks):     %llu", memInfoMalloc.smblks);
    LOGDEBUG1("# of mapped regions (hblks):           %llu", memInfoMalloc.hblks);
    LOGDEBUG1("Bytes in mapped regions (hblkhd):      %llu", memInfoMalloc.hblkhd);
    LOGDEBUG1("Max. total allocated space (usmblks):  %llu", memInfoMalloc.usmblks);
    LOGDEBUG1("Free bytes held in fastbins (fsmblks): %llu", memInfoMalloc.fsmblks);
    LOGDEBUG1("Total allocated space (uordblks):      %llu", memInfoMalloc.uordblks);
    LOGDEBUG1("Total free space (fordblks):           %llu", memInfoMalloc.fordblks);
    LOGDEBUG1("Topmost releasable block (keepcost):   %llu", memInfoMalloc.keepcost);

    return (((double)memInfoMalloc.uordblks + (double)memInfoMalloc.hblkhd) / 1024);
}

#else

double MemInfo::getUsedMemory()
{
    LOGWARN("MemInfo::getUsedMemory with GLIBC versions before 2.33 uses mallinfo and getrusage for estimation.");

    struct mallinfo memInfoMalloc = mallinfo();
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);

    double uordblks = static_cast<double>(memInfoMalloc.uordblks);
    double hblkhd = static_cast<double>(memInfoMalloc.hblkhd);
    double fordblks = static_cast<double>(memInfoMalloc.fordblks);

    // Check for overflow and adjust
    if (uordblks < 0) uordblks += static_cast<double>(INT_MAX);
    if (hblkhd < 0) hblkhd += static_cast<double>(INT_MAX);
    if (fordblks < 0) fordblks += static_cast<double>(INT_MAX);

    double freeBlocksSize = fordblks / 1024; // Convert to KB

    // Peak memory usage from getrusage
    double rusageMemory = usage.ru_maxrss; // ru_maxrss is already in KB

    LOGDEBUG1("Total allocated space (uordblks): %f KB", uordblks / 1024);
    LOGDEBUG1("Bytes in mapped regions (hblkhd): %f KB", hblkhd / 1024);
    LOGDEBUG1("Total free space (fordblks): %f KB", freeBlocksSize);
    LOGDEBUG1("Memory from getrusage: %f KB", rusageMemory);

    // Calculate used memory
    double usedMemory = rusageMemory - freeBlocksSize;

    // Ensure usedMemory is not negative
    if (usedMemory < 0) {
        usedMemory = 0;
    }

    LOGDEBUG1("Used memory after adjustment: %f KB", usedMemory);

    return usedMemory;
}

#endif

double MemInfo::getPickUsedMemory()
{
    struct rusage usage;

    getrusage(RUSAGE_SELF, &usage);
    return usage.ru_maxrss;
}

double MemInfo::getNotAvailableMemory()
{
    MemInfo *meminfo = MemInfo::getInstance();
    meminfo->parseMemInfo(); // Reparse the /proc/meminfo file
    return meminfo->MemTotal - meminfo->MemAvailable;
}

double MemInfo::getTotalMemory()
{
    MemInfo *meminfo = MemInfo::getInstance();
    return meminfo->MemTotal;
}

double MemInfo::getFreeMemory()
{
    MemInfo *meminfo = MemInfo::getInstance();
    meminfo->parseMemInfo(); // Reparse the /proc/meminfo file
    return meminfo->MemFree;
}

double MemInfo::getAvailableMemory()
{
    MemInfo *meminfo = MemInfo::getInstance();
    meminfo->parseMemInfo(); // Reparse the /proc/meminfo file
    return meminfo->MemAvailable;
}

// double getMemoryMax()
// {
// 	return sysconf(_SC_PHYS_PAGES) * (sysconf(_SC_PAGE_SIZE) / 1024);
// }

// double getMemoryFree()
// {
// 	return sysconf(_SC_AVPHYS_PAGES) * (sysconf(_SC_PAGE_SIZE) / 1024);
// }

// Time
// ====

static double timeStart = getAbsoluteTime();

double getAbsoluteTime()
{
    timeval time;

    gettimeofday(&time, NULL);

    return (double)time.tv_sec + (double)time.tv_usec * 0.000001;
}

double getRelativeTime()
{
    return getAbsoluteTime() - timeStart;
}

double getRelativeTime(double start)
{
    return getAbsoluteTime() - start;
}

void getTimeToWait(timespec *relativeTimeToWait, timespec *timeToWaitForCond)
{
    timeval now;
    gettimeofday(&now, NULL);
    timeToWaitForCond->tv_nsec = now.tv_usec * 1000 + relativeTimeToWait->tv_nsec;
    timeToWaitForCond->tv_sec = now.tv_sec + relativeTimeToWait->tv_sec;
    if (timeToWaitForCond->tv_nsec > BILLION)
    {
        timeToWaitForCond->tv_nsec = timeToWaitForCond->tv_nsec - BILLION;
        timeToWaitForCond->tv_sec++;
    }
    LOGDEBUG2("Now : %ld,%ld, relativeTimeTowait: %ld,%ld, result: %ld,%ld", now.tv_sec, now.tv_usec * 1000, relativeTimeToWait->tv_sec, relativeTimeToWait->tv_nsec, timeToWaitForCond->tv_sec, timeToWaitForCond->tv_nsec);
}

void getTimeSpecMicro(long microSeconds, timespec *result)
{
    result->tv_sec = microSeconds / MILLION;
    microSeconds = microSeconds - result->tv_sec * MILLION;
    result->tv_nsec = microSeconds * 1000;
}