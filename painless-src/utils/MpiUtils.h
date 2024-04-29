#pragma once

#include "ErrorCodes.h"

#define MYMPI_END 2012
#define MYMPI_CLAUSES 1
#define MYMPI_OK 2
#define MYMPI_NOTOK 3

#define COLOR_YES 10

#define TESTRUNMPI(func)           \
    do                             \
    {                              \
        if (func != MPI_SUCCESS)   \
        {                          \
            LOGERROR("MPI ERROR"); \
            exit(PERR_MPI);        \
        }                          \
    } while (0)
