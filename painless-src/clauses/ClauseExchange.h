// -----------------------------------------------------------------------------
// Copyright (C) 2017  Ludovic LE FRIOUX
//
// This file is part of PaInleSS.
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

#pragma once

#include <atomic>
#include <iostream>
#include <vector>

using namespace std;

/// Structure of a shared clause.
struct ClauseExchange
{
   /// Number of references on this clause.
   atomic<int> nbRefs;

   /// LBD value of this clause.
   int lbd;

   /// Id of the solver that has exported this clause.
   int from;

   /// Size of this clause.
   int size;

   /// Checksum of the clause
   size_t checksum;

   /// Literals of this clause.
   // int lits[0];
   std::vector<int> lits;

   // Comparing operators
   // friend bool operator==(const ClauseExchange &cls1, const ClauseExchange &cls2);
   // friend bool operator!=(const ClauseExchange &cls1, const ClauseExchange &cls2);

   // friend bool operator<(const ClauseExchange &cls1, const ClauseExchange &cls2);
   // friend bool operator>(const ClauseExchange &cls1, const ClauseExchange &cls2);

   // friend bool operator<=(const ClauseExchange &cls1, const ClauseExchange &cls2);
   // friend bool operator>=(const ClauseExchange &cls1, const ClauseExchange &cls2);
};

/// Print clause.
inline void printClauseExchange(ClauseExchange *cls);

// Compare two int vectors
bool operator==(const std::vector<int> &vector1, const std::vector<int> &vector2);