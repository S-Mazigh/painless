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

#include <stdlib.h>
#include <sys/resource.h>
// #include <stdio.h>
#include "utils/System.h"
#include "Logger.h"
#define BILLION 1000000000
#define MILLION 1000000

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

double getMemoryUsed()
{
	struct rusage r_usage;
	getrusage(RUSAGE_SELF, &r_usage);
	return r_usage.ru_maxrss;
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
	LOG(3, "Now : %ld,%ld, relativeTimeTowait: %ld,%ld, result: %ld,%ld\n", now.tv_sec, now.tv_usec * 1000, relativeTimeToWait->tv_sec, relativeTimeToWait->tv_nsec, timeToWaitForCond->tv_sec, timeToWaitForCond->tv_nsec);
}

void getTimeSpecMicro(long microSeconds, timespec *result)
{
	result->tv_sec = microSeconds / MILLION;
	microSeconds = microSeconds - result->tv_sec * MILLION;
	result->tv_nsec = microSeconds * 1000;
}