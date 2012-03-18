/*
    Texel - A UCI chess engine.
    Copyright (C) 2012  Peter Österlund, peterosterlund2@gmail.com

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
 * transpositionTableTest.cpp
 *
 *  Created on: Feb 25, 2012
 *      Author: petero
 */

#include "transpositionTableTest.hpp"
#include "transpositionTable.hpp"
#include "position.hpp"
#include "textio.hpp"
#include <iostream>

#include "cute.h"

typedef TranspositionTable::TTEntry TTEntry;

/**
 * Test of TTEntry nested class, of class TranspositionTable.
 */
static void
testTTEntry() {
    const int mate0 = SearchConst::MATE0;
    Position pos = TextIO::readFEN(TextIO::startPosFEN);
    Move move = TextIO::stringToMove(pos, "e4");

    // Test "normal" (non-mate) score
    int score = 17;
    int ply = 3;
    TTEntry ent1;
    ent1.key = 1;
    ent1.setMove(move);
    ent1.setScore(score, ply);
    ent1.setDepth(3);
    ent1.generation = 0;
    ent1.type = TType::T_EXACT;
    Move tmpMove;
    ent1.getMove(tmpMove);
    ASSERT_EQUAL(move, tmpMove);
    ASSERT_EQUAL(score, ent1.getScore(ply));
    ASSERT_EQUAL(score, ent1.getScore(ply + 3));    // Non-mate score, should be ply-independent

    // Test positive mate score
    TTEntry ent2;
    score = mate0 - 6;
    ply = 3;
    ent2.key = 3;
    move = Move(8, 0, Piece::BQUEEN);
    ent2.setMove(move);
    ent2.setScore(score, ply);
    ent2.setDepth(99);
    ent2.generation = 0;
    ent2.type = TType::T_EXACT;
    ent2.getMove(tmpMove);
    ASSERT_EQUAL(move, tmpMove);
    ASSERT_EQUAL(score, ent2.getScore(ply));
    ASSERT_EQUAL(score + 2, ent2.getScore(ply - 2));

    // Compare ent1 and ent2
    ASSERT(!ent1.betterThan(ent2, 0));  // More depth is good
    ASSERT(ent2.betterThan(ent1, 0));

    ent2.generation = 1;
    ASSERT(!ent2.betterThan(ent1, 0));  // ent2 has wrong generation
    ASSERT(ent2.betterThan(ent1, 1));   // ent1 has wrong generation

    ent2.generation = 0;
    ent1.setDepth(7); ent2.setDepth(7);
    ent1.type = TType::T_GE;
    ASSERT(ent2.betterThan(ent1, 0));
    ent2.type = TType::T_LE;
    ASSERT(!ent2.betterThan(ent1, 0));  // T_GE is equally good as T_LE
    ASSERT(!ent1.betterThan(ent2, 0));

    // Test negative mate score
    TTEntry ent3;
    score = -mate0 + 5;
    ply = 3;
    ent3.key = 3;
    move = Move(8, 0, Piece::BQUEEN);
    ent3.setMove(move);
    ent3.setScore(score, ply);
    ent3.setDepth(99);
    ent3.generation = 0;
    ent3.type = TType::T_EXACT;
    ent3.getMove(tmpMove);
    ASSERT_EQUAL(move, tmpMove);
    ASSERT_EQUAL(score, ent3.getScore(ply));
    ASSERT_EQUAL(score - 2, ent3.getScore(ply - 2));
}

/**
 * Test of insert method, of class TranspositionTable.
 */
static void
testInsert() {
    TranspositionTable tt(16);
    Position pos = TextIO::readFEN(TextIO::startPosFEN);
    std::string moves[] = {
        "e4", "e5", "Nf3", "Nc6", "Bb5", "a6", "Ba4", "b5", "Bb3", "Nf6", "O-O", "Be7", "Re1"
    };
    UndoInfo ui;
    for (size_t i = 0; i < COUNT_OF(moves); i++) {
        Move m = TextIO::stringToMove(pos, moves[i]);
        pos.makeMove(m, ui);
        int score = i * 17 + 3;
        m.setScore(score);
        int type = TType::T_EXACT;
        int ply = i + 1;
        int depth = i * 2 + 5;
        tt.insert(pos.historyHash(), m, type, ply, depth, score * 2 + 3);
    }

    pos = TextIO::readFEN(TextIO::startPosFEN);
    for (size_t i = 0; i < COUNT_OF(moves); i++) {
        Move m = TextIO::stringToMove(pos, moves[i]);
        pos.makeMove(m, ui);
        TranspositionTable::TTEntry ent;
        tt.probe(pos.historyHash(), ent);
        ASSERT_EQUAL(TType::T_EXACT, (int)ent.type);
        int score = i * 17 + 3;
        int ply = i + 1;
        int depth = i * 2 + 5;
        ASSERT_EQUAL(score, ent.getScore(ply));
        ASSERT_EQUAL(depth, ent.getDepth());
        ASSERT_EQUAL(score * 2 + 3, ent.evalScore);
        Move tmpMove;
        ent.getMove(tmpMove);
        ASSERT_EQUAL(m, tmpMove);
    }
}

cute::suite
TranspositionTableTest::getSuite() const {
    cute::suite s;
    s.push_back(CUTE(testTTEntry));
    s.push_back(CUTE(testInsert));
    return s;
}
