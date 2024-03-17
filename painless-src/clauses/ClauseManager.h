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
#include <stdlib.h>

#include "clauses/ClauseExchange.h"
#include "utils/BloomFilter.h"

using namespace std;

/// Class in charge of the management of share(d clauses.
class ClauseManager
{
public:
   /// Init the clause manager.
   static void initClauseManager()
   {
   }

   /// Alloc a new shared clause.
   static ClauseExchange *allocClause(int size)
   {
      ClauseExchange *ptr;
      ptr = new ClauseExchange(); //(ClauseExchange *)malloc(sizeof(ClauseExchange));
      ptr->lits.resize(size);
      ptr->size = size;
      ptr->nbRefs = 1;
      ptr->lbd = -1;
      ptr->from = -1;

      return ptr;
   }

   /// Alloc a ClauseExchange cpp version (Maybe should make a classic cpp class ?)
   static ClauseExchange *initClause(std::vector<int> &&v_cls, int lbd)
   {
      int size = v_cls.size();
      ClauseExchange *ptr;
      ptr = new ClauseExchange();
      // faire un ptr->lits.reserve(v_cls.size()) pour eviter les allocations dynamique du std vector
      ptr->size = size;
      ptr->nbRefs = 1;
      ptr->from = -1;
      ptr->lbd = lbd;
      ptr->lits = v_cls;
      ptr->checksum = lookup3_hash_clause(ptr->lits);

      return ptr;
   }

   /// Alloc a ClauseExchange cpp version (Maybe should make a classic cpp class ?)
   static ClauseExchange *initClause(std::vector<int> &&v_cls, int lbd, int from)
   {
      int size = v_cls.size();
      ClauseExchange *ptr;
      ptr = new ClauseExchange();
      // faire un ptr->lits.reserve(v_cls.size()) pour eviter les allocations dynamique du std vector
      ptr->size = size;
      ptr->nbRefs = 1;
      ptr->from = from;
      ptr->lbd = lbd;
      ptr->lits = v_cls;
      ptr->checksum = lookup3_hash_clause(ptr->lits);

      return ptr;
   }

   /// Increase the number of references to a shared clause.
   static void increaseClause(ClauseExchange *cls, int refs = 1)
   {
      cls->nbRefs += refs; // atomic adition
   }

   /// Release a shared clauses, delete it if needed.
   static void releaseClause(ClauseExchange *cls)
   {
      // std::cout << "\t -" << cls << " : " << (int)(cls->nbRefs - 1) << std::endl;
      int oldValue = cls->nbRefs.fetch_sub(1); // atomic decrementation

      if (oldValue - 1 <= 0)
      {
         // Only the last thread should execute this code
         delete cls;
      }
   }

   /// Join the clause manager.
   static void joinClauseManager()
   {
   }
};
