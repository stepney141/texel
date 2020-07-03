/*
    Texel - A UCI chess engine.
    Copyright (C) 2012-2016  Peter Österlund, peterosterlund2@gmail.com

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
 * evaluateTest.cpp
 *
 *  Created on: Feb 25, 2012
 *      Author: petero
 */

#include "evaluateTest.hpp"
#include "positionTest.hpp"
#include "evaluate.hpp"
#include "position.hpp"
#include "textio.hpp"
#include "parameters.hpp"

#include "gtest/gtest.h"

int swapSquareX(int square) {
    int x = Square::getX(square);
    int y = Square::getY(square);
    return Square::getSquare(7-x, y);
}

int swapSquareY(int square) {
    int x = Square::getX(square);
    int y = Square::getY(square);
    return Square::getSquare(x, 7-y);
}

Position
swapColors(const Position& pos) {
    Position sym;
    sym.setWhiteMove(!pos.isWhiteMove());
    for (int x = 0; x < 8; x++) {
        for (int y = 0; y < 8; y++) {
            int sq = Square::getSquare(x, y);
            int p = pos.getPiece(sq);
            p = Piece::isWhite(p) ? Piece::makeBlack(p) : Piece::makeWhite(p);
            sym.setPiece(swapSquareY(sq), p);
        }
    }

    int castleMask = 0;
    if (pos.a1Castle()) castleMask |= 1 << Position::A8_CASTLE;
    if (pos.h1Castle()) castleMask |= 1 << Position::H8_CASTLE;
    if (pos.a8Castle()) castleMask |= 1 << Position::A1_CASTLE;
    if (pos.h8Castle()) castleMask |= 1 << Position::H1_CASTLE;
    sym.setCastleMask(castleMask);

    if (pos.getEpSquare() >= 0)
        sym.setEpSquare(swapSquareY(pos.getEpSquare()));

    sym.setHalfMoveClock(pos.getHalfMoveClock());
    sym.setFullMoveCounter(pos.getFullMoveCounter());

    return sym;
}

/** Mirror position in X direction, remove castling rights. */
Position mirrorX(const Position& pos) {
    Position mir;
    mir.setWhiteMove(pos.isWhiteMove());
    for (int x = 0; x < 8; x++) {
        for (int y = 0; y < 8; y++) {
            int sq = Square::getSquare(x, y);
            int p = pos.getPiece(sq);
            mir.setPiece(swapSquareX(sq), p);
        }
    }

    if (pos.getEpSquare() >= 0)
        mir.setEpSquare(swapSquareX(pos.getEpSquare()));

    mir.setHalfMoveClock(pos.getHalfMoveClock());
    mir.setFullMoveCounter(pos.getFullMoveCounter());

    return mir;
}

/** Evaluation position and check position serialization. */
static int
evalPos(Evaluate& eval, const Position& pos, bool evalMirror, bool testMirror) {
    {
        Position pos1(pos);
        U64 h1 = pos1.historyHash();
        pos1.computeZobristHash();
        U64 h2 = pos1.historyHash();
        EXPECT_EQ(h1, h2);
    }

    Position pos2;
    Position::SerializeData data;
    pos.serialize(data);
    pos2.deSerialize(data);
    EXPECT_EQ(pos, pos2);
    EXPECT_EQ(pos.wMtrl(), pos2.wMtrl());
    EXPECT_EQ(pos.bMtrl(), pos2.bMtrl());
    EXPECT_EQ(pos.wMtrlPawns(), pos2.wMtrlPawns());
    EXPECT_EQ(pos.bMtrlPawns(), pos2.bMtrlPawns());

    int evalScore = eval.evalPos(pos);

    if (evalMirror) {
        Position mir = mirrorX(pos);
        int mirrorEval = evalPos(eval, mir, false, false);
        if (testMirror) {
            EXPECT_LE(std::abs(evalScore - mirrorEval), 2);
        }
    }

    return evalScore;
}

int evalPos(Evaluate& eval, const Position& pos) {
    return evalPos(eval, pos, true, true);
}

/** Return static evaluation score for white, regardless of whose turn it is to move. */
int
evalWhite(const Position& pos, bool testMirror) {
    static auto et = Evaluate::getEvalHashTables();
    Evaluate eval(*et);
    return evalWhite(eval, pos, testMirror);
}

int
evalWhite(Evaluate& eval, const Position& pos, bool testMirror) {
    int ret = evalPos(eval, pos, true, testMirror);
    std::string fen = TextIO::toFEN(pos);
    Position symPos = swapColors(pos);
    std::string symFen = TextIO::toFEN(symPos);
    int symScore = evalPos(eval, symPos, true, testMirror);
    EXPECT_EQ(ret, symScore) << (fen + " == " + symFen);
    EXPECT_EQ(pos.materialId(), PositionTest::computeMaterialId(pos));
    EXPECT_EQ(symPos.materialId(), PositionTest::computeMaterialId(symPos));
    EXPECT_EQ(MatId::mirror(pos.materialId()), symPos.materialId());
    EXPECT_EQ(pos.materialId(), MatId::mirror(symPos.materialId()));
    if (!pos.isWhiteMove())
        ret = -ret;
    return ret;
}

/** Compute change in eval score for white after making "moveStr" in position "pos". */
static int
moveScore(const Position& pos, const std::string& moveStr) {
    int score1 = evalWhite(pos);
    Position tmpPos(pos);
    UndoInfo ui;
    tmpPos.makeMove(TextIO::stringToMove(tmpPos, moveStr), ui);
    int score2 = evalWhite(tmpPos);
//    printf("move:%s s1:%d s2:%d\n", moveStr, score1, score2);
    return score2 - score1;
}

static int
evalFEN(const std::string& fen, bool testMirror = false) {
    Position pos = TextIO::readFEN(fen);
    return evalWhite(pos, testMirror);
}

TEST(EvaluateTest, testEvalPos) {
    EvaluateTest::testEvalPos();
}

void
EvaluateTest::testEvalPos() {
    Position pos = TextIO::readFEN(TextIO::startPosFEN);
    UndoInfo ui;
    pos.makeMove(TextIO::stringToMove(pos, "e4"), ui);
    pos.makeMove(TextIO::stringToMove(pos, "e5"), ui);
    pos.makeMove(TextIO::stringToMove(pos, "Nf3"), ui);
    pos.makeMove(TextIO::stringToMove(pos, "Nc6"), ui);
    pos.makeMove(TextIO::stringToMove(pos, "Bb5"), ui);
    pos.makeMove(TextIO::stringToMove(pos, "Nge7"), ui);
    EXPECT_GE(moveScore(pos, "O-O"), 0);     // Castling is good
    EXPECT_LT(moveScore(pos, "Ke2"), 0);     // Losing right to castle is bad
    EXPECT_LT(moveScore(pos, "Kf1"), 0);
    EXPECT_LT(moveScore(pos, "Rg1"), 0);
    EXPECT_LT(moveScore(pos, "Rf1"), 0);

    pos = TextIO::readFEN("8/8/8/1r3k2/4pP2/4P3/8/4K2R w K - 0 1");
    EXPECT_EQ(true, pos.h1Castle());
    int cs1 = evalWhite(pos);
    pos.setCastleMask(pos.getCastleMask() & ~(1 << Position::H1_CASTLE));
    EXPECT_EQ(false, pos.h1Castle());
    int cs2 = evalWhite(pos);
    EXPECT_GE(cs2, cs1 - 7);    // No bonus for useless castle right

    // Test rook open file bonus
    pos = TextIO::readFEN("r4rk1/1pp1qppp/3b1n2/4p3/2B1P1b1/1QN2N2/PP3PPP/R3R1K1 w - - 0 1");
    int ms1 = moveScore(pos, "Red1");
    int ms2 = moveScore(pos, "Rec1");
    int ms3 = moveScore(pos, "Rac1");
    int ms4 = moveScore(pos, "Rad1");
    EXPECT_GT(ms1, 0);        // Good to have rook on open file
//    EXPECT_GT(ms2, 0);        // Good to have rook on half-open file
    EXPECT_GT(ms1, ms2);      // Open file better than half-open file
    EXPECT_GE(ms3, -3);
    EXPECT_GT(ms4, 0);
//    EXPECT_GT(ms4, ms1);
//    EXPECT_GT(ms3, ms2);

    pos = TextIO::readFEN("r3kb1r/p3pp1p/bpPq1np1/4N3/2pP4/2N1PQ2/P1PB1PPP/R3K2R b KQkq - 0 12");
    EXPECT_GT(moveScore(pos, "O-O-O"), 0);    // Black long castle is bad for black
    pos.makeMove(TextIO::stringToMove(pos, "O-O-O"), ui);
    EXPECT_GT(moveScore(pos, "O-O"), 0);      // White short castle is good for white

    pos = TextIO::readFEN("rnbqkbnr/pppp1ppp/8/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 0 1");
    EXPECT_GT(moveScore(pos, "O-O"), 0);      // Short castle is good for white

    pos = TextIO::readFEN("8/3k4/2p5/1pp5/1P1P4/3K4/8/8 w - - 0 1");
    int sc1 = moveScore(pos, "bxc5");
    int sc2 = moveScore(pos, "dxc5");
    EXPECT_LT(sc1, sc2);      // Don't give opponent a passed pawn.

    pos = TextIO::readFEN("8/pp1bk3/8/8/8/8/PPPBK3/8 w - - 0 1");
    sc1 = evalWhite(pos);
    pos.setPiece(Square::getSquare(3, 1), Piece::EMPTY);
    pos.setPiece(Square::getSquare(3, 2), Piece::WBISHOP);
    sc2 = evalWhite(pos);
    EXPECT_GT(sc2, sc1);      // Easier to win if bishops on same color

    // Test bishop mobility
    pos = TextIO::readFEN("r1bqkbnr/pppp1ppp/2n5/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3");
    sc1 = moveScore(pos, "Bd3") - protectBonus[1];
    sc2 = moveScore(pos, "Bc4");
    EXPECT_GT(sc2, sc1);
}

TEST(EvaluateTest, testPieceSquareEval) {
    EvaluateTest::testPieceSquareEval();
}

void
EvaluateTest::testPieceSquareEval() {
    Position pos = TextIO::readFEN(TextIO::startPosFEN);
    int score = evalWhite(pos);
    EXPECT_EQ(tempoBonusMG, score);      // Should be zero + tempo bonus, by symmetry
    UndoInfo ui;
    pos.makeMove(TextIO::stringToMove(pos, "e4"), ui);
    score = evalWhite(pos);
    EXPECT_GT(score, 0);     // Centralizing a pawn is a good thing
    pos.makeMove(TextIO::stringToMove(pos, "e5"), ui);
    score = evalWhite(pos);
    EXPECT_EQ(tempoBonusMG, score);      // Should be zero + tempo bonus, by symmetry
    EXPECT_GT(moveScore(pos, "Nf3"), 0);      // Developing knight is good
    pos.makeMove(TextIO::stringToMove(pos, "Nf3"), ui);
    EXPECT_LT(moveScore(pos, "Nc6"), 0);      // Developing knight is good
    pos.makeMove(TextIO::stringToMove(pos, "Nc6"), ui);
    EXPECT_GT(moveScore(pos, "Bb5"), 0);      // Developing bishop is good
    pos.makeMove(TextIO::stringToMove(pos, "Bb5"), ui);
    pos.makeMove(TextIO::stringToMove(pos, "Nge7"), ui);
    score = evalWhite(pos);
    pos.makeMove(TextIO::stringToMove(pos, "Bxc6"), ui);
    pos.makeMove(TextIO::stringToMove(pos, "Nxc6"), ui);
    int score2 = evalWhite(pos);
    EXPECT_LT(score2, score);                 // Bishop worth more than knight in this case
//    EXPECT_GE(moveScore(pos, "Qe2"), -9);    // Queen away from edge is good

    pos = TextIO::readFEN("5k2/4nppp/p1n5/1pp1p3/4P3/2P1BN2/PP3PPP/3R2K1 w - - 0 1");
    EXPECT_GT(moveScore(pos, "Rd7"), 0);      // Rook on 7:th rank is good
//    EXPECT_GE(moveScore(pos, "Rd8"), 0);      // Rook on 8:th rank also good
    pos.setPiece(TextIO::getSquare("a1"), Piece::WROOK);
    pos.setPiece(TextIO::getSquare("d1"), Piece::EMPTY);
    EXPECT_GE(moveScore(pos, "Rac1") + tempoBonusMG, 0); // Rook on c-f files considered good

    pos = TextIO::readFEN("r4rk1/pppRRppp/1q4b1/n7/8/2N3B1/PPP1QPPP/6K1 w - - 0 1");
    score = evalWhite(pos);
    EXPECT_GT(score, 100); // Two rooks on 7:th rank is very good
}

TEST(EvaluateTest, testTradeBonus) {
    EvaluateTest::testTradeBonus();
}

void
EvaluateTest::testTradeBonus() {
    std::string fen = "8/5k2/6r1/2p1p3/3p4/2P2N2/3PPP2/4K1R1 w - - 0 1";
    Position pos = TextIO::readFEN(fen);
    int score1 = evalWhite(pos);
    UndoInfo ui;
    pos.makeMove(TextIO::stringToMove(pos, "Rxg6"), ui);
    pos.makeMove(TextIO::stringToMove(pos, "Kxg6"), ui);
    int score2 = evalWhite(pos);
    EXPECT_GT(score2, score1);    // White ahead, trading pieces is good

    pos = TextIO::readFEN(fen);
    pos.makeMove(TextIO::stringToMove(pos, "cxd4"), ui);
    pos.makeMove(TextIO::stringToMove(pos, "cxd4"), ui);
    score2 = evalWhite(pos);
    EXPECT_LT(score2, score1);    // White ahead, trading pawns is bad

    pos = TextIO::readFEN("8/8/1b2b3/4kp2/5N2/4NKP1/6B1/8 w - - 0 62");
    score1 = evalWhite(pos);
    pos.makeMove(TextIO::stringToMove(pos, "Nxe6"), ui);
    pos.makeMove(TextIO::stringToMove(pos, "Kxe6"), ui);
    score2 = evalWhite(pos);
    EXPECT_GT(score2, score1); // White ahead, trading pieces is good
}

static int material(const Position& pos) {
    return pos.wMtrl() - pos.bMtrl();
}

TEST(EvaluateTest, testMaterial) {
    EvaluateTest::testMaterial();
}

void
EvaluateTest::testMaterial() {
    Position pos = TextIO::readFEN(TextIO::startPosFEN);
    EXPECT_EQ(0, material(pos));

    const int pV = ::pV;
    const int qV = ::qV;
    EXPECT_NE(pV, 0);
    EXPECT_NE(qV, 0);
    EXPECT_GT(qV, pV);

    UndoInfo ui;
    pos.makeMove(TextIO::stringToMove(pos, "e4"), ui);
    EXPECT_EQ(0, material(pos));
    pos.makeMove(TextIO::stringToMove(pos, "d5"), ui);
    EXPECT_EQ(0, material(pos));
    pos.makeMove(TextIO::stringToMove(pos, "exd5"), ui);
    EXPECT_EQ(pV, material(pos));
    pos.makeMove(TextIO::stringToMove(pos, "Qxd5"), ui);
    EXPECT_EQ(0, material(pos));
    pos.makeMove(TextIO::stringToMove(pos, "Nc3"), ui);
    EXPECT_EQ(0, material(pos));
    pos.makeMove(TextIO::stringToMove(pos, "Qxd2"), ui);
    EXPECT_EQ(-pV, material(pos));
    pos.makeMove(TextIO::stringToMove(pos, "Qxd2"), ui);
    EXPECT_EQ(-pV+qV, material(pos));

    int s1 = evalFEN("6k1/ppp2pp1/1nnnnn1p/8/8/7P/PPP2PP1/3QQ1K1 w - - 0 1");
    EXPECT_LT(s1, 0);
    int s2 = evalFEN("6k1/ppp2pp1/nnnnnnnp/8/8/7P/PPP2PP1/Q2QQ1K1 w - - 0 1");
    EXPECT_LT(s2, s1);
    int s3 = evalFEN("nnnnknnn/pppppppp/8/8/8/8/PPPPPPPP/Q2QK2Q w - - 0 1");
    EXPECT_LT(s3, 55);

    // Test symmetry of imbalances corrections
    evalFEN("3rr1k1/pppb1ppp/2n2n2/4p3/1bB1P3/2N1BN2/PPP1QPPP/6K1 w - - 0 1");
    evalFEN("3q1rk1/1p1bppbp/p2p1np1/8/1n1NP1PP/2Q1BP2/PPP1B3/1K1R3R w - - 0 1");
    evalFEN("r1bqkbnr/1pp2ppp/p1p5/4p3/4P3/5N2/PPPP1PPP/RNBQK2R b KQkq - 0 1");
    evalFEN("r1bqkbnr/1p3ppp/p7/4p3/4P3/5N2/PPPP1PPP/RNBQK2R b KQkq - 0 1");
    evalFEN("r1bqkbnr/1pp2ppp/p1p5/4p3/4P3/5N2/P2P1PPP/RNBQK2R b KQkq - 0 1");
    evalFEN("r1bq4/pppp1kpp/2n2n2/2b1p3/4P3/8/PPPP1PPP/RNBQ1RK1 w - - 0 1");
}

static void movePiece(Position& pos, const std::string& from, const std::string& to) {
    int f = TextIO::getSquare(from);
    int t = TextIO::getSquare(to);
    int p = pos.getPiece(f);
    pos.setPiece(f, Piece::EMPTY);
    pos.setPiece(t, p);
}

TEST(EvaluateTest, testKingSafety) {
    EvaluateTest::testKingSafety();
}

void
EvaluateTest::testKingSafety() {
    Position pos = TextIO::readFEN("r3kb1r/p1p1pppp/b2q1n2/4N3/3P4/2N1PQ2/P2B1PPP/R3R1K1 w kq - 0 1");
    int s1 = evalWhite(pos);
    movePiece(pos, "g7", "b7");
    int s2 = evalWhite(pos);
    EXPECT_LT(s2, s1);    // Half-open g-file is bad for white

    // Trapping rook with own king is bad
    pos = TextIO::readFEN("rnbqk1nr/pppp1ppp/8/8/1bBpP3/8/PPP2PPP/RNBQK1NR w KQkq - 2 4");
    s1 = evalWhite(pos);
    pos = TextIO::readFEN("rnbqk1nr/pppp1ppp/8/8/1bBpP3/8/PPP2PPP/RNBQ1KNR w kq - 2 4");
    s2 = evalWhite(pos);
    EXPECT_LT(s2, s1 + 3);

//    pos = TextIO::readFEN("rnbqk1nr/pppp1ppp/8/8/1bBpPB2/8/PPP1QPPP/RN1K2NR w kq - 0 1");
//    s1 = evalWhite(pos);
//    movePiece(pos, "d1", "c1"); // rnbqk1nr/pppp1ppp/8/8/1bBpPB2/8/PPP1QPPP/RNK3NR w kq - 0 1
//    s2 = evalWhite(pos);
//    EXPECT_LE(s2, s1 + 8);

    // Opposite castling
    pos = TextIO::readFEN("rnbq1rk1/1p2ppbp/p2p1np1/8/3NP3/2N1BP2/PPPQ2PP/2KR1B1R w - - 0 1");
    int sKc1Ph2 = evalWhite(pos);
    movePiece(pos, "c1", "b1");
    int sKb1Ph2 = evalWhite(pos);
    movePiece(pos, "h2", "h3");
    int sKb1Ph3 = evalWhite(pos);
    movePiece(pos, "b1", "c1");
    int sKc1Ph3 = evalWhite(pos);
    EXPECT_LE(std::abs((sKb1Ph3 - sKb1Ph2) - (sKc1Ph3 - sKc1Ph2)), 2); // Pawn storm bonus same if own king moves within its flank

    int sKg8Ph3 = evalWhite(pos);
    movePiece(pos, "h3", "h2");
    int sKg8Ph2 = evalWhite(pos);
    movePiece(pos, "g8", "h8");
    int sKh8Ph2 = evalWhite(pos);
    movePiece(pos, "h2", "h3");
    int sKh8Ph3 = evalWhite(pos);
    EXPECT_LE(std::abs((sKg8Ph3 - sKg8Ph2) - (sKh8Ph3 - sKh8Ph2)), 2); // Pawn storm bonus same if other king moves within its flank

    // Test symmetry of king safety evaluation
    evalFEN("rnbq1r1k/pppp1ppp/4pn2/2b5/8/5NP1/PPPPPPBP/RNBQ1RK1 w - - 0 1");
    evalFEN("rn3r1k/pppq1ppp/3p1n2/2b1p3/8/5NPb/PPPPPPBP/RNBQ1RK1 w - - 0 1");
    evalFEN("rn3r1k/ppp2ppp/3p1n2/2b1p3/4P1q1/5bP1/PPPP1PNP/RNB1QRK1 w - - 0 1");
    evalFEN("rn3r1k/ppp1b1pp/3p1n2/2b1p3/4P1q1/5pP1/PPPP1P1P/RNB1QRKB w - - 0 1");
}

static void
evalEGConsistency(const std::string& fen, const std::string& wSq, int wPiece,
                  const std::string& bSq, int bPiece, int fuzz) {
    Position pos = TextIO::readFEN(fen);
    int s00 = evalWhite(pos);
    std::string f00 = TextIO::toFEN(pos);
    pos.setPiece(TextIO::getSquare(wSq), wPiece);
    int s10 = evalWhite(pos);
    std::string f10 = TextIO::toFEN(pos);
    pos.setPiece(TextIO::getSquare(bSq), bPiece);
    int s11 = evalWhite(pos);
    std::string f11 = TextIO::toFEN(pos);
    pos.setPiece(TextIO::getSquare(wSq), Piece::EMPTY);
    int s01 = evalWhite(pos);
    std::string f01 = TextIO::toFEN(pos);
    EXPECT_GE(s10, s00 - fuzz) << (f10 + " >= " + f00);
    EXPECT_LE(s01, s00 + fuzz) << (f01 + " <= " + f00);
    EXPECT_GE(s10, s11 - fuzz) << (f10 + " >= " + f11);
    EXPECT_LE(s01, s11 + fuzz) << (f01 + " <= " + f11);
}

static int
evalEgFen(const std::string& fen, int fuzz = 0) {
    for (int wp = Piece::WQUEEN; wp <= Piece::WPAWN; wp++) {
        for (int bp = Piece::BQUEEN; bp <= Piece::BPAWN; bp++) {
            evalEGConsistency(fen, "a2", wp, "a7", bp, fuzz);
            for (int wp2 = Piece::WQUEEN; wp2 <= Piece::WPAWN; wp2++) {
                for (int bp2 = Piece::BQUEEN; bp2 <= Piece::BPAWN; bp2++) {
                    Position pos = TextIO::readFEN(fen);
                    pos.setPiece(TextIO::getSquare("a2"), wp);
                    evalEGConsistency(TextIO::toFEN(pos), "b2", wp2, "b7", bp2, fuzz);
                    pos.setPiece(TextIO::getSquare("a7"), bp);
                    evalEGConsistency(TextIO::toFEN(pos), "b2", wp2, "b7", bp2, fuzz);
                    pos.setPiece(TextIO::getSquare("a2"), Piece::EMPTY);
                    evalEGConsistency(TextIO::toFEN(pos), "b2", wp2, "b7", bp2, fuzz);
                }
            }
        }
    }
    return evalFEN(fen);
}

TEST(EvaluateTest, testEndGameEval) {
    EvaluateTest::testEndGameEval();
}

void
EvaluateTest::testEndGameEval() {
    Position pos;
    pos.setPiece(Square::getSquare(4, 1), Piece::WKING);
    pos.setPiece(Square::getSquare(4, 6), Piece::BKING);
    int score = evalWhite(pos, true);
    EXPECT_EQ(tempoBonusEG, score);

    pos.setPiece(Square::getSquare(3, 1), Piece::WBISHOP);
    score = evalWhite(pos, true);
    EXPECT_EQ(tempoBonusEG, score); // Insufficient material to mate

    pos.setPiece(Square::getSquare(3, 1), Piece::WKNIGHT);
    score = evalWhite(pos, true);
    EXPECT_EQ(tempoBonusEG, score); // Insufficient material to mate

    pos.setPiece(Square::getSquare(3, 1), Piece::WROOK);
    score = evalWhite(pos, true);
    const int rV = ::rV;
    EXPECT_GT(std::abs(score), rV + 90);   // Enough material to force mate

    pos.setPiece(Square::getSquare(3, 6), Piece::BBISHOP);
    score = evalWhite(pos, true);
    const int bV = ::bV;
    EXPECT_GE(score, 0);
    EXPECT_LT(score, rV - bV);   // Insufficient excess material to mate

    pos.setPiece(Square::getSquare(5, 6), Piece::BROOK);
    score = evalWhite(pos, true);
    EXPECT_LE(score, 0);
    EXPECT_LT(-score, bV);

    pos.setPiece(Square::getSquare(2, 6), Piece::BBISHOP);
    score = evalWhite(pos, true);
    EXPECT_GT(-score, bV * 2);

    // KRPKB is win for white
    score = evalFEN("8/3bk3/8/8/8/3P4/3RK3/8 w - - 0 1", true);
    const int pV = ::pV;
    EXPECT_GT(score, rV + pV - bV - 100);

    // KNNK is a draw
    score = evalFEN("8/8/4k3/8/8/3NK3/3N4/8 w - - 0 1", true);
    EXPECT_EQ(tempoBonusEG, score);

    const int nV = ::nV;
    score = evalFEN("8/8/8/4k3/N6N/P2K4/8/8 b - - 0 66", true);
    EXPECT_GT(score, nV * 2);

    pos = TextIO::readFEN("8/8/3k4/8/8/3NK3/2B5/8 b - - 0 1");
    score = evalWhite(pos, true);
    EXPECT_GT(score, 560);  // KBNK is won
    score = moveScore(pos, "Kc6");
    EXPECT_GT(score, 0);      // Black king going into wrong corner, good for white
    score = moveScore(pos, "Ke6");
    EXPECT_LT(score, tempoBonusEG*2); // Black king going away from wrong corner, good for black

    // KRN vs KR is generally drawn
    score = evalFEN("rk/p/8/8/8/8/NKR/8 w - - 0 1", true);
    EXPECT_LT(score, nV - 2 * pV);

    // KRKB, defending king should prefer corner that bishop cannot attack
    pos = TextIO::readFEN("6B1/8/8/8/8/2k5/4r3/2K5 w - - 0 93");
    score = evalWhite(pos, true);
    EXPECT_GE(score, -pV);
    score = moveScore(pos, "Kd1");
    EXPECT_LT(score, 0);
    score = moveScore(pos, "Kb1");
    EXPECT_GT(score + tempoBonusEG, 0);

    // Position with passed pawn and opposite side has a knight
    score = evalFEN("8/8/8/1P6/8/B7/1K5n/7k w - - 0 1");
    EXPECT_GT(score, pV);

    { // Test KRPKM
        int score1 = evalFEN("8/2b5/k7/P7/RK6/8/8/8 w - - 0 1", true);
        EXPECT_LT(score1, 170);
        int score2 = evalFEN("8/1b6/k7/P7/RK6/8/8/8 w - - 0 1", true);
        EXPECT_GT(score2, 300);
        int score3 = evalFEN("8/3b4/1k6/1P6/1RK5/8/8/8 w - - 0 1", true);
        EXPECT_GT(score3, 300);
        int score4 = evalFEN("8/3n4/1k6/1P6/1RK5/8/8/8 w - - 0 1", true);
        EXPECT_GT(score4, 400);
        int score5 = evalFEN("8/2n5/k7/P7/RK6/8/8/8 w - - 0 1", true);
        EXPECT_GT(score5, 400);
    }

    { // Test KQKRM+pawns
        int score = evalFEN("8/3pk3/2b1r3/4P3/3QK3/8/8/8 w - - 0 1");
        EXPECT_LT(score, pV / 2);
        score = evalFEN("8/3pk3/2b2r2/5P2/3Q1K2/8/8/8 w - - 0 1");
        EXPECT_GT(score, pV);
        score = evalFEN("8/3p1k2/2b2r2/8/5P2/3QK3/8/8 w - - 0 1");
        EXPECT_GT(score, pV);
        score = evalFEN("8/3p1k2/2b5/8/8/5r2/3QKP2/8 w - - 0 1");
        EXPECT_LT(score, pV / 2);
        score = evalFEN("8/4pk2/5b2/6p1/3r2Pp/8/2Q1K2P/8 w - - 0 1");
        EXPECT_LT(score, pV / 5);
        score = evalFEN("8/4pk2/5b2/6p1/3r3p/8/2Q1K1PP/8 w - - 0 1");
        EXPECT_GT(score, pV / 2);
        score = evalFEN("8/4p1k1/4Pb2/5Pp1/3r2Pp/8/2Q1K2P/8 w - - 0 1");
        EXPECT_LT(score, pV);
        score = evalFEN("8/4p1k1/4Pb2/5Pp1/3r2Pp/3P4/2Q1K2P/8 w - - 0 1");
        EXPECT_LT(score, pV);
        score = evalFEN("7k/3p4/2p5/1r3b2/8/8/1P1Q1P2/4K3 w - - 0 1");
        EXPECT_LT(score, pV / 2);
        score = evalFEN("8/5p2/5Bp1/1k3qP1/3R4/4K3/8/8 w - - 0 1");
        EXPECT_GT(score, -pV / 2);

        score = evalFEN("7k/3p4/2p1n3/2P5/3r4/2QP1K2/8/8 w - - 0 1");
        EXPECT_LT(score, pV / 2);
        score = evalFEN("7k/3p4/2p1n3/2P5/3r4/2Q2K2/4P3/8 w - - 0 1");
        EXPECT_GT(score, pV * 2);
        score = evalFEN("8/3p1k2/2p1n3/2P5/3rP3/2Q2K2/8/8 w - - 0 1");
        EXPECT_LT(score, pV / 2);
    }

    { // Test KQKNNNN
        int score = evalFEN("3nk3/3nnn2/8/8/3QK3/8/8/8 w - - 0 1");
        EXPECT_LT(score, -250);
        score = evalFEN("8/5K2/8/3nk3/3nnn2/8/1Q6/8 b - - 0 1");
        EXPECT_LT(score, -450);
        evalEgFen("3nk3/3nnn2/8/8/3QK3/8/8/8 w - - 0 1");
    }
}

TEST(EvaluateTest, testEndGameSymmetry) {
    EvaluateTest::testEndGameSymmetry();
}

void
EvaluateTest::testEndGameSymmetry() {
    // Test symmetry for pawnless endings
    {
        int score1 = evalFEN("8/8/3rk3/8/8/8/8/3QK3 w - - 0 1", true);
        int score2 = evalFEN("8/8/8/Q4r2/K4k2/8/8/8 w - - 0 1", true);
        EXPECT_EQ(score2, score1);
        int score3 = evalFEN("3KQ3/8/8/8/8/3kr3/8/8 w - - 0 1", true);
        EXPECT_EQ(score3, score1);
        int score4 = evalFEN("8/8/8/2k4K/2r4Q/8/8/8 w - - 0 1", true);
        EXPECT_EQ(score4, score1);
    }
    {
        int score1 = evalFEN("8/8/3rk3/8/8/8/8/3RK3 w - - 0 1", true);
        int score2 = evalFEN("8/8/8/R4r2/K4k2/8/8/8 w - - 0 1", true);
        EXPECT_EQ(score2, score1);
        int score3 = evalFEN("3KR3/8/8/8/8/3kr3/8/8 w - - 0 1", true);
        EXPECT_EQ(score3, score1);
        int score4 = evalFEN("8/8/8/2k4K/2r4R/8/8/8 w - - 0 1", true);
        EXPECT_EQ(score4, score1);
    }
}

/** Return true if score is within tempoBonus of 0. */
static bool isDraw(int score) {
    if (abs(score) <= tempoBonusEG) {
        return true;
    } else {
        std::cout << "score:" << score << std::endl;
        return false;
    }
}

TEST(EvaluateTest, testEndGameCorrections) {
    EvaluateTest::testEndGameCorrections();
}

void
EvaluateTest::testEndGameCorrections() {
    // Four bishops on same color can not win
    int score = evalEgFen("8/4k3/8/1B6/2B5/3B4/2K1B3/8 w - - 0 1", 1);
    EXPECT_TRUE(isDraw(score));
    // Two bishops on same color can not win against knight
    score = evalEgFen("8/3nk3/8/8/2B5/3B4/4K3/8 w - - 0 1");
    EXPECT_LE(score, 0 + tempoBonusEG);

    int kqk = evalEgFen("8/4k3/8/8/8/3QK3/8/8 w - - 0 1");
    EXPECT_GT(kqk, 1275);

    int krk = evalEgFen("8/4k3/8/8/8/3RK3/8/8 w - - 0 1");
    EXPECT_GT(krk, 930);
    int kqkn = evalEgFen("8/3nk3/8/8/8/3QK3/8/8 w - - 0 1", 2);
    EXPECT_GT(kqkn, 960);
    int kqkb = evalEgFen("8/3bk3/8/8/8/3QK3/8/8 w - - 0 1", 3);
    EXPECT_GT(kqkb, 960);

    EXPECT_GT(kqk, krk);
    EXPECT_GT(kqk, kqkn);
    EXPECT_GT(kqk, kqkb);

    int kbbk = evalEgFen("8/4k3/8/8/8/2BBK3/8/8 w - - 0 1", 6);
    EXPECT_GE(kbbk, 775);

    EXPECT_GT(krk, kbbk);
    EXPECT_GT(kqkn, kbbk);
    EXPECT_GT(kqkb, kbbk);

    int kbnk = evalEgFen("8/4k3/8/8/8/2BNK3/8/8 w - - 0 1");
    EXPECT_GT(kbnk, 475);
    EXPECT_LT(kbnk, 650);
    int kqkr = evalEgFen("8/3rk3/8/8/8/3QK3/8/8 w - - 0 1");
    EXPECT_GT(kqkr, 475);
    EXPECT_LT(kqkr, 675);

    EXPECT_GT(kbbk, kbnk);
    EXPECT_GT(kbbk, kqkr);

    int kqkbn = evalEgFen("8/2bnk3/8/8/8/3QK3/8/8 w - - 0 1");
    EXPECT_GE(kqkbn, 200);
    EXPECT_LE(kqkbn, 250);

    EXPECT_GT(kbnk, kqkbn);
    EXPECT_GT(kqkr, kqkbn);

    int kbbkn = evalEgFen("8/3nk3/8/8/8/2BBK3/8/8 w - - 0 1");
    EXPECT_GT(kbbkn, 75);
    EXPECT_LT(kbbkn, 125);

    EXPECT_GT(kqkbn, kbbkn);

    int kqknn = evalEgFen("8/2nnk3/8/8/8/3QK3/8/8 w - - 0 1");
    EXPECT_GT(kqknn, 25);
    EXPECT_LT(kqknn, 75);
    int kqkbb = evalEgFen("8/2bbk3/8/8/8/3QK3/8/8 w - - 0 1");
    EXPECT_GT(kqkbb, 25);
    EXPECT_LT(kqkbb, 75);
    int kbbkb = evalEgFen("8/3bk3/8/8/8/2BBK3/8/8 w - - 0 1", 1);
    EXPECT_GT(kbbkb, 25);
    EXPECT_LT(kbbkb, 75);
    int kbnkb = evalEgFen("8/3bk3/8/8/8/2NBK3/8/8 w - - 0 1");
    EXPECT_GT(kbnkb, 25);
    EXPECT_LT(kbnkb, 75);
    int kbnkn = evalEgFen("8/3nk3/8/8/8/2NBK3/8/8 w - - 0 1");
    EXPECT_GT(kbnkn, 25);
    EXPECT_LT(kbnkn, 75);
    int knnkb = evalEgFen("8/3bk3/8/8/8/2NNK3/8/8 w - - 0 1");
    EXPECT_GT(knnkb, 0);
    EXPECT_LT(knnkb, 50);
    int knnkn = evalEgFen("8/3nk3/8/8/8/2NNK3/8/8 w - - 0 1");
    EXPECT_GT(knnkn, 0);
    EXPECT_LT(knnkn, 50);

    EXPECT_GT(kbbkn, kqknn);
    EXPECT_GT(kbbkn, kqkbb);
    EXPECT_GT(kbbkn, kbbkb);
    EXPECT_GT(kbbkn, kbnkb);
    EXPECT_GT(kbbkn, kbnkn);
    EXPECT_GT(kbbkn, knnkb);
    EXPECT_GT(kbbkn, knnkn);

    int krkb = evalEgFen("8/3bk3/8/8/8/3RK3/8/8 w - - 0 1", 1);
    EXPECT_GT(krkb, 0);
    EXPECT_LT(krkb, 50);
    int krkn = evalEgFen("8/3nk3/8/8/8/3RK3/8/8 w - - 0 1", 1);
    EXPECT_GT(krkn, 0);
    EXPECT_LT(krkn, 50);

    // KRKBNN is a draw
    int kbnnkr = evalEgFen("8/3rk3/8/8/8/3N4/2NBK3/8 w - - 0 1");
    EXPECT_GE(kbnnkr, 0);
    EXPECT_LT(kbnnkr, 50);

    score = evalFEN("4k3/8/4R1n1/4Pn2/8/8/P2K2b1/8 b - - 6 1", true);
    EXPECT_GE(score, -50);

    // KRKBBN is a win for the BBN side
    int kbbnkr = evalEgFen("8/3rk3/8/8/8/3B4/2NBK3/8 w - - 0 1");
    EXPECT_GE(kbbnkr, 300);

    // KRBNKRB is a generally a win
    int krbnkrb = evalEgFen("8/4k3/3br3/8/8/3RBN2/4K3/8 w - - 0 1", 0);
    EXPECT_GT(krbnkrb, 200);
    EXPECT_LT(krbnkrb, 300);

    // KRRMKRR is generally a win, except that the 50 move rule
    // sometimes makes it a draw
    int krrnkrr = evalEgFen("8/5r2/3r4/4k3/2R4R/4K3/4N3/8 w - -", 0);
    EXPECT_GT(krrnkrr, 200);
    EXPECT_LT(krrnkrr, 300);
    int krrbkrr = evalEgFen("8/5r2/3r4/4k3/2R4R/4K3/4B3/8 w - -", 0);
    EXPECT_GT(krrbkrr, 200);
    EXPECT_LT(krrbkrr, 300);
}

TEST(EvaluateTest, testPassedPawns) {
    EvaluateTest::testPassedPawns();
}

void
EvaluateTest::testPassedPawns() {
    Position pos = TextIO::readFEN("8/8/8/P3k/8/8/p/K w");
    int score = evalWhite(pos);
    EXPECT_GE(score, 29); // Unstoppable passed pawn
    pos.setWhiteMove(false);
    score = evalWhite(pos);
    EXPECT_LE(score, 0); // Not unstoppable
    EXPECT_GT(evalFEN("8/8/P2k4/8/8/8/p7/K7 w - - 0 1"), 88); // Unstoppable passed pawn

    pos = TextIO::readFEN("4R3/8/8/p2K4/P7/4pk2/8/8 w - - 0 1");
    score = evalWhite(pos);
    pos.setPiece(TextIO::getSquare("d5"), Piece::EMPTY);
    pos.setPiece(TextIO::getSquare("d4"), Piece::WKING); // 4R3/8/8/p7/P2K4/4pk2/8/8 w - - 0 1
    int score2 = evalWhite(pos);
    EXPECT_GE(score2, score - 6); // King closer to passed pawn promotion square

    pos = TextIO::readFEN("4R3/8/8/3K4/8/4pk2/8/8 w - - 0 1");
    score = evalWhite(pos);
    pos.setPiece(TextIO::getSquare("d5"), Piece::EMPTY);
    pos.setPiece(TextIO::getSquare("d4"), Piece::WKING);
    score2 = evalWhite(pos);
    EXPECT_GT(score2, score); // King closer to passed pawn promotion square

    // Connected passed pawn test. Disabled because it didn't help in tests
    //        pos = TextIO::readFEN("4k3/8/8/4P3/3P1K2/8/8/8 w - - 0 1");
    //        score = evalWhite(pos);
    //        pos.setPiece(TextIO::getSquare("d4"), Piece::EMPTY);
    //        pos.setPiece(TextIO::getSquare("d5"), Piece::WPAWN);
    //        score2 = evalWhite(pos);
    //        EXPECT_GT(score2, score); // Advancing passed pawn is good

    // Test symmetry of candidate passed pawn evaluation
    evalFEN("rnbqkbnr/p1pppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    evalFEN("rnbqkbnr/p2ppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    evalFEN("rnbqkbnr/p2ppppp/8/P7/8/8/1PPPPPPP/RNBQKBNR w KQkq - 0 1");
    evalFEN("rnbqkbnr/p2ppppp/8/P2P4/8/2P5/1P2PPPP/RNBQKBNR w KQkq - 0 1");
    evalFEN("rnbqkbnr/pp1ppppp/8/P2P4/8/2P5/1P2PPPP/RNBQKBNR w KQkq - 0 1");
    evalFEN("rnbqkbnr/pp1ppppp/8/PP1P4/8/2P5/4PPPP/RNBQKBNR w KQkq - 0 1");
    evalFEN("rnbqkbnr/p2ppppp/8/PP6/8/2P5/4PPPP/RNBQKBNR w KQkq - 0 1");
    evalFEN("rnbqkbnr/p2ppppp/8/P2P4/8/2P5/4PPPP/RNBQKBNR w KQkq - 0 1");

    // Test symmetry of "king supporting passed pawn" evaluation
    evalFEN("8/6K1/4R3/7p/2q5/5p1Q/5k2/8 w - - 2 89");
}

TEST(EvaluateTest, testBishAndRookPawns) {
    EvaluateTest::testBishAndRookPawns();
}

void
EvaluateTest::testBishAndRookPawns() {
    const int bV = ::bV;
    const int winScore = bV;
    const int drawish = bV / 20;
    Position pos = TextIO::readFEN("k7/8/8/8/2B5/2K5/P7/8 w - - 0 1");
    EXPECT_GT(evalWhite(pos, true), winScore);

    pos = TextIO::readFEN("k7/8/8/8/3B4/2K5/P7/8 w - - 0 1");
    EXPECT_LT(evalWhite(pos, true), drawish);

    pos = TextIO::readFEN("8/2k5/8/8/3B4/2K5/P7/8 w - - 0 1");
    EXPECT_GT(evalWhite(pos, true), winScore);

    pos = TextIO::readFEN("8/2k5/8/8/3B4/2K4P/8/8 w - - 0 1");
    EXPECT_GT(evalWhite(pos, true), winScore);

    pos = TextIO::readFEN("8/2k5/8/8/4B3/2K4P/8/8 w - - 0 1");
    EXPECT_GT(evalWhite(pos, true), winScore);

    pos = TextIO::readFEN("8/6k1/8/8/4B3/2K4P/8/8 w - - 0 1");
    EXPECT_LT(evalWhite(pos, true), drawish);

    pos = TextIO::readFEN("8/6k1/8/8/4B3/2K4P/7P/8 w - - 0 1");
    EXPECT_LT(evalWhite(pos, true), drawish);

    pos = TextIO::readFEN("8/6k1/8/8/2B1B3/2K4P/7P/8 w - - 0 1");
    EXPECT_LT(evalWhite(pos, true), drawish);

    pos = TextIO::readFEN("8/6k1/8/2B5/4B3/2K4P/7P/8 w - - 0 1");
    EXPECT_GT(evalWhite(pos, true), winScore);

    pos = TextIO::readFEN("8/6k1/8/8/4B3/2K4P/P7/8 w - - 0 1");
    EXPECT_GT(evalWhite(pos, true), winScore);

    pos = TextIO::readFEN("8/6k1/8/8/4B3/2K3PP/8/8 w - - 0 1");
    EXPECT_GT(evalWhite(pos, true), winScore);
}

TEST(EvaluateTest, testBishAndPawnFortress) {
    EvaluateTest::testBishAndPawnFortress();
}

void
EvaluateTest::testBishAndPawnFortress() {
    EXPECT_TRUE(isDraw(evalFEN("1k5B/1p6/1P6/3K4/8/8/8/8 w - - 0 1", true)));
    EXPECT_TRUE(isDraw(evalFEN("k6B/1p6/1P6/3K4/8/8/8/8 w - - 0 1", true)));
    EXPECT_GT(evalFEN("4k3/1p6/1P3B2/3K4/8/8/8/8 w - - 0 1", true), 0);

    EXPECT_TRUE(isDraw(evalFEN("2k4B/1pP5/1P6/3K4/8/8/8/8 w - - 0 1", true)));
    EXPECT_TRUE(isDraw(evalFEN("7B/1pPk4/1P6/3K4/8/8/8/8 w - - 0 1", true)));
    EXPECT_GT(evalFEN("k6B/1pP5/1P6/3K4/8/8/8/8 w - - 0 1", true), 0);
    EXPECT_TRUE(isDraw(evalFEN("2k4B/1pP5/1P6/3K2B1/1P6/8/8/8 w - - 0 1", true)));
    EXPECT_GT(evalFEN("2k4B/1pP5/1P6/3K4/1P6/3B4/8/8 w - - 0 1", true), 0);

    EXPECT_GT(evalFEN("nk5B/1p6/1P6/1P6/1P6/1P3K2/1P6/8 w - - 0 1", true), 0);
    EXPECT_TRUE(isDraw(evalFEN("rk5B/1p6/1P5B/1P4B1/1P6/1P3K2/1P6/8 w - - 0 1", true)));
    EXPECT_TRUE(isDraw(evalFEN("1k5B/1p6/1P6/1P6/1P6/1P3K2/1P6/7n w - - 0 1", true)));

    EXPECT_TRUE(isDraw(evalFEN("r1k4B/1pP5/1P6/3K4/1P6/8/3B4/8 w - - 0 1", true)));
    EXPECT_GT(evalFEN("n1k4B/1pP5/1P6/3K4/1P6/8/3B4/8 w - - 0 1", true), 0);

    EXPECT_TRUE(isDraw(evalFEN("2k5/1p6/1P6/4B1K1/8/8/8/8 b - - 0 1", true)));
    EXPECT_GT(evalFEN("2k5/Kp6/1P6/4B3/8/8/8/8 b - - 0 1", true), 0);
    EXPECT_TRUE(isDraw(evalFEN("k7/1pK5/1P6/8/3B4/8/8/8 w - - 0 1", true)));
    EXPECT_TRUE(isDraw(evalFEN("3k4/1p6/1P6/5K2/3B4/8/8/8 w - - 0 1", true)));
    EXPECT_GT(evalFEN("1K1k4/1p6/1P6/8/3B4/8/8/8 w - - 0 1", true), 0);

    EXPECT_LT(evalFEN("8/8/6p1/2b5/2k2P1P/6p1/6P1/7K w - - 1 1", true), 0);
    EXPECT_LT(evalFEN("8/8/6p1/2b5/2k4P/6pP/6P1/7K w - - 1 1", true), 0);

    EXPECT_TRUE(isDraw(evalFEN("8/8/8/8/7p/4k1p1/5bP1/5K2 w - - 1 1", true)));
    EXPECT_LT(evalFEN("8/8/8/8/7p/4k1p1/5bP1/5K2 b - - 1 1", true), 0);
    EXPECT_GT(evalFEN("2k5/1pB5/1P3K2/P7/8/8/8/8 b - - 1 1", true), 0);
    EXPECT_GT(evalFEN("2k5/1p6/1P1BK3/P7/8/8/8/8 b - - 1 1", true), 0);
    EXPECT_TRUE(isDraw(evalFEN("2k1K3/1p6/1P6/P7/8/6B1/8/8 b - - 1 1", true)));
    EXPECT_TRUE(isDraw(evalFEN("k1K3/1p6/1P6/P7/8/8/5B2/8 b - - 1 1", true)));
    EXPECT_GT(evalFEN("k3K3/1p6/1P6/P7/8/8/5B2/8 b - - 1 1", true), 0);
    EXPECT_TRUE(isDraw(evalFEN("k3K3/1p6/1P6/P7/8/8/7B/8 b - - 1 1", true)));
    EXPECT_TRUE(isDraw(evalFEN("k7/1pK5/1P6/P7/8/8/7B/8 b - - 1 1", true)));
    EXPECT_TRUE(isDraw(evalFEN("k7/1pK5/1P6/P7/8/4B3/8/8 b - - 1 1", true)));
    EXPECT_TRUE(isDraw(evalFEN("k1K5/1p6/1P6/P7/8/4B3/8/8 b - - 1 1", true)));
    EXPECT_LT(evalFEN("8/8/8/2b5/4k2p/4P1p1/6P1/7K w - - 1 1", true), 0);
    EXPECT_TRUE(isDraw(evalFEN("8/4b3/4P3/8/7p/6p1/5kP1/7K w - - 1 2", true)));
    EXPECT_TRUE(isDraw(evalFEN("8/8/8/2b1k3/4P2p/6p1/6P1/7K w - - 1 1", true)));

    EXPECT_TRUE(isDraw(evalFEN("8/8/8/8/6p1/6p1/4k1P1/6K1 b - - 0 10", true)));
    EXPECT_TRUE(isDraw(evalFEN("8/6p1/6p1/8/6p1/8/4k1P1/6K1 b - - 0 1", true)));
    EXPECT_LT(evalFEN("8/6p1/6p1/8/6p1/6P1/4k1K1/8 b - - 0 1", true), 0);

    EXPECT_TRUE(isDraw(evalFEN("7k/5K2/6P1/8/8/3B4/8/8 b - - 1 1", true)));
    EXPECT_TRUE(isDraw(evalFEN("7k/1B3K2/6P1/8/8/3B4/8/8 b - - 1 1", true)));
    EXPECT_GT(evalFEN("7k/5K2/6P1/8/3B4/8/8/8 b - - 1 1", true), 500);
    EXPECT_GT(evalFEN("7k/5KP1/6P1/8/8/3B4/8/8 b - - 1 1", true), 700);
    EXPECT_GT(evalFEN("7k/5K2/6P1/8/8/3B4/8/8 w - - 1 1", true), 500);
    EXPECT_GT(evalFEN("8/5K1k/6P1/8/8/3B4/8/8 b - - 1 1", true), 500);
    EXPECT_GT(evalFEN("7k/5K2/8/6P1/2B5/8/8/8 b - - 1 1", true), 500);

    EXPECT_TRUE(isDraw(evalFEN("8/Bk6/1P6/2K5/8/8/8/8 b - - 0 1", true)));
    EXPECT_TRUE(isDraw(evalFEN("k7/B7/1P6/8/8/5K2/8/8 b - - 0 1", true)));
    EXPECT_TRUE(isDraw(evalFEN("k7/B7/1PK5/8/8/8/8/8 b - - 0 1", true)));
    EXPECT_GT(evalFEN("k7/B7/1PK5/8/8/8/8/8 w - - 0 1", true), 500);
    EXPECT_TRUE(isDraw(evalFEN("k7/B7/1P6/3K4/8/8/8/8 w - - 0 1", true)));

    EXPECT_TRUE(isDraw(evalFEN("6k1/6Pp/7P/8/3B4/3K4/8/8 w - - 0 1", true)));
    EXPECT_TRUE(isDraw(evalFEN("6k1/6Pp/7P/8/3B4/3K4/8/8 b - - 0 1", true)));
    EXPECT_TRUE(isDraw(evalFEN("6k1/6Pp/7P/8/3B4/3K3P/8/8 w - - 0 1", true)));
    EXPECT_TRUE(isDraw(evalFEN("6k1/6Pp/7P/8/3B4/3K3P/8/8 b - - 0 1", true)));
    EXPECT_TRUE(isDraw(evalFEN("8/5kPp/7P/7P/3B4/3K4/8/8 w - - 0 1", true)));
    EXPECT_TRUE(isDraw(evalFEN("8/5kPp/7P/7P/3B4/3K4/8/8 b - - 0 1", true)));
    EXPECT_GT(evalFEN("6k1/6Pp/8/7P/3B4/3K4/8/8 w - - 0 1", true), 500);
    EXPECT_GT(evalFEN("6k1/6Pp/8/7P/3B4/3K4/8/8 b - - 0 1", true), 500);
    EXPECT_GT(evalFEN("8/5kPp/7P/7P/3B4/2BK4/8/8 w - - 0 1", true), 500);
    EXPECT_GT(evalFEN("8/5kPp/7P/8/3B4/3K2P1/8/8 w - - 0 1", true), 500);
    EXPECT_GT(evalFEN("8/5kPp/7P/8/3B4/3K4/1P6/8 w - - 0 1", true), 500);
    EXPECT_GT(evalFEN("8/5kPp/7P/8/8/3K4/2B5/8 w - - 0 1", true), 500);
    EXPECT_GT(evalFEN("6k1/6Pp/8/8/8/3K4/3B4/8 w - - 0 1", true), 400);
    EXPECT_GT(evalFEN("6k1/6P1/7P/8/8/3K4/3B4/8 w - - 0 1", true), 500);
    EXPECT_TRUE(isDraw(evalFEN("6k1/7p/7P/8/8/3K4/3B4/8 w - - 0 1", true)));
    EXPECT_TRUE(isDraw(evalFEN("8/5k1p/7P/8/8/3K4/3B4/8 w - - 0 1", true)));
    EXPECT_TRUE(isDraw(evalFEN("7k/7p/7P/8/8/3K4/3B4/8 w - - 0 1", true)));
    EXPECT_TRUE(isDraw(evalFEN("6k1/1p4Pp/7P/8/3B4/3K4/8/8 w - - 0 1", true)));
    EXPECT_TRUE(isDraw(evalFEN("6k1/1p4Pp/7P/8/3B4/3K3P/8/8 w - - 0 1", true)));
    EXPECT_GT(evalFEN("6k1/6Pp/6pP/8/3B4/3K3P/8/8 w - - 0 1", true), 500);
    EXPECT_TRUE(isDraw(evalFEN("5k2/3p3p/5K1P/7P/3B3P/8/8/8 w - - 0 1", true)));
    EXPECT_TRUE(isDraw(evalFEN("6k1/6Pp/7P/8/3BK3/8/6pP/8 w - - 0 1", true)));
    EXPECT_GT(evalFEN("6k1/6Pp/7P/6p1/3BK1pP/8/8/8 w - - 0 1", true), 500);
    EXPECT_TRUE(isDraw(evalFEN("6k1/6Pp/7P/6pP/3BK1p1/8/8/8 w - - 0 1", true)));
}

TEST(EvaluateTest, testTrappedBishop) {
    EvaluateTest::testTrappedBishop();
}

void
EvaluateTest::testTrappedBishop() {
    Position pos = TextIO::readFEN("r2q1rk1/ppp2ppp/3p1n2/8/3P4/1P1Q1NP1/b1P2PBP/2KR3R w - - 0 1");
    EXPECT_GT(evalWhite(pos), -15); // Black has trapped bishop

    pos = TextIO::readFEN("r2q2k1/pp1b1p1p/2p2np1/3p4/3P4/1BNQ2P1/PPPB1P1b/2KR4 w - - 0 1");
    EXPECT_GT(evalWhite(pos), -pV/2); // Black has trapped bishop
}

TEST(EvaluateTest, testKQKP) {
    EvaluateTest::testKQKP();
}

void
EvaluateTest::testKQKP() {
    const int pV = ::pV;
    const int qV = ::qV;
    const int winScore = 350;
    const int drawish = (pV + qV) / 20;

    // Pawn on a2
    Position pos = TextIO::readFEN("8/8/1K6/8/8/Q7/p7/1k6 w - - 0 1");
    EXPECT_LT(evalWhite(pos), drawish);
    pos = TextIO::readFEN("8/8/8/1K6/8/Q7/p7/1k6 w - - 0 1");
    EXPECT_GT(evalWhite(pos), winScore);
    pos = TextIO::readFEN("3Q4/8/8/8/K7/8/1kp5/8 w - - 0 1");
    EXPECT_GT(evalWhite(pos), winScore);
    pos = TextIO::readFEN("8/8/8/8/8/1Q6/p3K3/k7 b - - 0 1");
    EXPECT_LT(evalWhite(pos), drawish);
    pos = TextIO::readFEN("3Q4/2K5/8/8/8/k7/p7/8 w - - 0 1");
    EXPECT_GT(evalWhite(pos), winScore);

    // Pawn on c2
    pos = TextIO::readFEN("3Q4/8/8/8/3K4/8/1kp5/8 w - - 0 1");
    EXPECT_LT(evalWhite(pos), drawish);
    pos = TextIO::readFEN("3Q4/8/8/8/8/4K3/1kp5/8 w - - 0 1");
    EXPECT_GT(evalWhite(pos), winScore);

    EXPECT_GT(evalFEN("8/8/8/4K3/8/8/2Q5/k7 w - - 0 1"), 0);
    EXPECT_TRUE(isDraw(evalFEN("8/8/8/4K3/8/8/2Q5/k7 b - - 0 1")));
}

TEST(EvaluateTest, testKQKRP) {
    EvaluateTest::testKQKRP();
}

void
EvaluateTest::testKQKRP() {
    EXPECT_LT(evalWhite(TextIO::readFEN("1k6/1p6/2r5/8/1K2Q3/8/8/8 w - - 0 1")), 50);
    EXPECT_GT(evalWhite(TextIO::readFEN("8/2k5/2p5/3r4/4Q3/2K5/8/8 w - - 0 1")), 200);
    EXPECT_LT(evalWhite(TextIO::readFEN("1k6/1p6/p1r5/8/1K6/4Q3/8/8 w - - 0 1")), 50);
    EXPECT_LT(evalWhite(TextIO::readFEN("1k6/1p6/1pr5/8/1K6/4Q3/8/8 w - - 0 1")), 50);
    EXPECT_LT(evalWhite(TextIO::readFEN("6k1/6p1/5rp1/8/6K1/3Q4/8/8 w - - 0 1")), 50);
    EXPECT_LT(evalWhite(TextIO::readFEN("8/8/8/3k4/8/3p2Q1/4r3/5K2 b - - 0 1")), 50);
    EXPECT_LT(evalWhite(TextIO::readFEN("8/8/8/8/2Q5/3pk3/4r3/5K2 w - - 0 1")), 50);
    EXPECT_GT(evalWhite(TextIO::readFEN("8/8/8/4Q3/8/3pk3/4r3/5K2 b - - 0 1")), 48);
    EXPECT_LT(evalWhite(TextIO::readFEN("8/8/8/2k5/8/2p2Q2/3r4/4K3 b - - 3 2")), 25);
    EXPECT_GT(evalWhite(TextIO::readFEN("1k6/8/1p6/2r5/3K4/8/4Q3/8 w - - 0 1")), 100);
    EXPECT_LT(evalWhite(TextIO::readFEN("1k6/8/1p6/2r5/3K4/8/5Q2/8 w - - 0 1")), 50);
    EXPECT_LT(evalWhite(TextIO::readFEN("8/8/8/5Q2/8/1kp5/3r4/4K3 w - - 0 1")), 10);
    EXPECT_GT(evalWhite(TextIO::readFEN("8/8/8/1Q6/8/1kp5/3r4/2K5 b - - 0 1")), 25);
    EXPECT_LT(evalWhite(TextIO::readFEN("8/8/8/8/Q7/2pk4/3r4/2K5 b - - 0 1")), 10);
    EXPECT_GT(evalWhite(TextIO::readFEN("8/8/8/3Q4/8/2pk4/3r4/2K5 b - - 0 1")), 25);
}

TEST(EvaluateTest, testKRKP) {
    EvaluateTest::testKRKP();
}

void
EvaluateTest::testKRKP() {
    const int pV = ::pV;
    const int rV = ::rV;
    const int winScore = 343;
    const int drawish = (pV + rV) / 20;
    Position pos = TextIO::readFEN("6R1/8/8/8/5K2/2kp4/8/8 w - - 0 1");
    EXPECT_GT(evalWhite(pos), winScore);
    pos.setWhiteMove(!pos.isWhiteMove());
    EXPECT_LT(evalWhite(pos), drawish);
}

TEST(EvaluateTest, testKRPKR) {
    EvaluateTest::testKRPKR();
}

void
EvaluateTest::testKRPKR() {
    const int pV = ::pV;
    const int winScore = 2 * pV;
    const int drawish = pV * 2 / 3;
    Position pos = TextIO::readFEN("8/r7/4K1k1/4P3/8/5R2/8/8 w - - 0 1");
    EXPECT_GT(evalWhite(pos), winScore);

    pos = TextIO::readFEN("4k3/7R/1r6/5K2/4P3/8/8/8 w - - 0 1");
    EXPECT_LT(evalWhite(pos), drawish);
}

TEST(EvaluateTest, testKPK) {
    EvaluateTest::testKPK();
}

void
EvaluateTest::testKPK() {
    const int pV = ::pV;
    const int rV = ::rV;
    const int winScore = rV - pV;
    const int drawish = (pV + rV) / 20;
    Position pos = TextIO::readFEN("8/8/8/3k4/8/8/3PK3/8 w - - 0 1");
    EXPECT_GT(evalWhite(pos), winScore);
    pos.setWhiteMove(!pos.isWhiteMove());
    EXPECT_LT(evalWhite(pos), drawish);
}

TEST(EvaluateTest, testKPKP) {
    EvaluateTest::testKPKP();
}

void
EvaluateTest::testKPKP() {
    EXPECT_TRUE(isDraw(evalFEN("1k6/1p6/1P6/3K4/8/8/8/8 w - - 0 1")));
    EXPECT_TRUE(isDraw(evalFEN("3k4/1p6/1P6/3K4/8/8/8/8 w - - 0 1")));
    EXPECT_GT(evalFEN("2k5/Kp6/1P6/8/8/8/8/8 w - - 0 1"), 0);
}

TEST(EvaluateTest, testKBNK) {
    EvaluateTest::testKBNK();
}

void
EvaluateTest::testKBNK() {
    int s1 = evalWhite(TextIO::readFEN("B1N5/1K6/8/8/8/2k5/8/8 b - - 0 1"));
    EXPECT_GT(s1, 550);
    int s2 = evalWhite(TextIO::readFEN("1BN5/1K6/8/8/8/2k5/8/8 b - - 1 1"));
    EXPECT_GT(s2, s1);
    int s3 = evalWhite(TextIO::readFEN("B1N5/1K6/8/8/8/2k5/8/8 b - - 0 1"));
    EXPECT_LT(s3, s2);
    int s4 = evalWhite(TextIO::readFEN("B1N5/1K6/8/8/8/5k2/8/8 b - - 0 1"));
    EXPECT_GT(s4, s3);

    int s5 = evalWhite(TextIO::readFEN("B1N5/8/8/8/8/4K2k/8/8 b - - 0 1"));
    int s6 = evalWhite(TextIO::readFEN("B1N5/8/8/8/8/5K1k/8/8 b - - 0 1"));
    EXPECT_GT(s6, s5);
}

TEST(EvaluateTest, testKBPKB) {
    EvaluateTest::testKBPKB();
}

void
EvaluateTest::testKBPKB() {
    const int pV = ::pV;
    const int drawish = pV / 5;
    int score = evalWhite(TextIO::readFEN("8/3b4/3k4/8/3P4/3B4/3K4/8 w - - 0 1"));
    EXPECT_GE(score, 0);
    EXPECT_LT(score, drawish);

    score = evalWhite(TextIO::readFEN("8/1b1k4/8/3PK3/8/3B4/8/8 w - - 0 1"));
    EXPECT_GE(score, 0);
    EXPECT_LT(score, pV); // Close to known draw

    score = evalWhite(TextIO::readFEN("8/1b6/7k/8/P7/KB6/8/8 w - - 0 1"));
    EXPECT_GT(score, pV);

    score = evalWhite(TextIO::readFEN("8/4k3/P1K5/8/8/4b3/B7/8 w - - 0 1"));
    EXPECT_GE(score, 0);
    EXPECT_LT(score, drawish);

    score = evalWhite(TextIO::readFEN("1b6/4k3/P1K5/8/8/8/B7/8 w - - 0 1"));
    EXPECT_GT(score, pV / 3);

    score = evalWhite(TextIO::readFEN("1b6/4k3/2K5/P7/8/8/B7/8 w - - 0 1"));
    EXPECT_GE(score, 0);
    EXPECT_LT(score, drawish);

    score = evalWhite(TextIO::readFEN("8/1P3k2/8/8/K3b3/B7/8/8 w - - 0 1"));
    EXPECT_GE(score, pV / 3);
}

TEST(EvaluateTest, testKBPKN) {
    EvaluateTest::testKBPKN();
}

void
EvaluateTest::testKBPKN() {
    const int pV = ::pV;
    const int drawish = pV / 5;
    int score = evalWhite(TextIO::readFEN("8/3k4/8/3P3n/2KB4/8/8/8 w - - 0 1"));
    EXPECT_GT(score, pV);

    score = evalWhite(TextIO::readFEN("8/3k4/8/3P4/2KB3n/8/8/8 w - - 0 1"));
    EXPECT_GE(score, 0);
    EXPECT_LT(score, drawish);

    score = evalWhite(TextIO::readFEN("8/3k4/8/3P4/2KB2n1/8/8/8 w - - 0 1"));
    EXPECT_GE(score, 0);
    EXPECT_LT(score, drawish);

    score = evalWhite(TextIO::readFEN("2k5/8/8/3P4/2KB2n1/8/8/8 w - - 0 1"));
    EXPECT_GE(score, 0);
    EXPECT_LT(score, pV);

    score = evalWhite(TextIO::readFEN("2k5/8/8/3P3n/2KB4/8/8/8 w - - 0 1"));
    EXPECT_GT(score, pV);

    score = evalWhite(TextIO::readFEN("2k5/8/8/3P4/2KB3n/8/8/8 w - - 0 1"));
    EXPECT_GE(score, 0);
    EXPECT_LT(score, pV);
}

TEST(EvaluateTest, testKNPKB) {
    EvaluateTest::testKNPKB();
}

void
EvaluateTest::testKNPKB() {
    const int pV = ::pV;
    const int drawish = pV / 5;
    int score = evalWhite(TextIO::readFEN("8/8/3b4/3P4/3NK3/8/8/7k w - - 0 1"));
    EXPECT_GE(score, 0);
    EXPECT_LT(score, drawish);

    score = evalWhite(TextIO::readFEN("8/8/3P4/8/3NK3/b7/8/7k w - - 0 1"));
    EXPECT_GT(score, pV);

    score = evalWhite(TextIO::readFEN("8/8/8/3P4/4K3/4N3/b7/7k w - - 0 1"));
    EXPECT_LT(score, drawish);

    score = evalWhite(TextIO::readFEN("8/8/8/8/1K6/P3N3/b7/7k w - - 0 1"));
    EXPECT_GT(score, pV);

    score = evalWhite(TextIO::readFEN("8/3P4/4b3/4N3/3K1k2/8/8/8 b - - 0 1"));
    EXPECT_TRUE(isDraw(score));
    score = evalWhite(TextIO::readFEN("8/3P4/4b3/4N3/3K1k2/8/8/8 w - - 0 1"));
    EXPECT_GT(score, pV);

    score = evalWhite(TextIO::readFEN("8/3P4/4Nk2/8/3K4/7b/8/8 b - - 0 1"));
    EXPECT_GT(score, pV);

    score = evalWhite(TextIO::readFEN("8/3P4/3N4/8/3K2k1/7b/8/8 b - - 0 1"));
    EXPECT_GT(score, pV);
}

TEST(EvaluateTest, testKNPK) {
    EvaluateTest::testKNPK();
}

void
EvaluateTest::testKNPK() {
    const int pV = ::pV;
    const int nV = ::nV;
    int score = evalWhite(TextIO::readFEN("k7/P7/8/1N6/1K6/8/8/8 w - - 0 1"));
    EXPECT_TRUE(isDraw(score));
    score = evalWhite(TextIO::readFEN("8/Pk6/8/1N6/1K6/8/8/8 w - - 0 1"));
    EXPECT_TRUE(isDraw(score));

    score = evalWhite(TextIO::readFEN("k7/8/P7/1N6/1K6/8/8/8 w - - 0 1"));
    EXPECT_GT(score, nV);

    score = evalWhite(TextIO::readFEN("K7/P1k5/8/5N2/8/8/8/8 w - - 0 1"));
    EXPECT_GT(score, pV + nV);
    score = evalWhite(TextIO::readFEN("K7/P1k5/8/5N2/8/8/8/8 b - - 0 1"));
    EXPECT_TRUE(isDraw(score));

    score = evalWhite(TextIO::readFEN("K7/P1k5/8/8/7N/8/8/8 b - - 0 1"));
    EXPECT_GT(score, nV - pV);
    score = evalWhite(TextIO::readFEN("K7/P1k5/8/8/7N/8/8/8 w - - 0 1"));
    EXPECT_TRUE(isDraw(score));

    score = evalWhite(TextIO::readFEN("K7/P3k3/8/8/7N/8/8/8 w - - 0 1"));
    EXPECT_GT(score, pV + nV);
    score = evalWhite(TextIO::readFEN("K7/P3k3/8/8/7N/8/8/8 b - - 0 1"));
    EXPECT_GT(score, pV + nV);
}

TEST(EvaluateTest, testCantWin) {
    EvaluateTest::testCantWin();
}

void
EvaluateTest::testCantWin() {
    Position pos = TextIO::readFEN("8/8/8/3k4/3p4/3K4/4N3/8 w - - 0 1");
    int score1 = evalWhite(pos);
    EXPECT_LE(score1, tempoBonusEG);
    UndoInfo ui;
    pos.makeMove(TextIO::stringToMove(pos, "Nxd4"), ui);
    int score2 = evalWhite(pos);
    EXPECT_LE(score2, 0);
    EXPECT_GE(score2, score1 - 2 * tempoBonusEG);
}

TEST(EvaluateTest, testPawnRace) {
    EvaluateTest::testPawnRace();
}

void
EvaluateTest::testPawnRace() {
    const int pV = ::pV;
    const int winScore = 130;
    const int drawish = 78;
    Position pos = TextIO::readFEN("8/8/K7/1P3p2/8/6k1/8/8 w - - 0 1");
    EXPECT_GT(evalWhite(pos), winScore);
    pos = TextIO::readFEN("8/8/K7/1P3p2/8/6k1/8/8 b - - 0 1");
    EXPECT_GT(evalWhite(pos), winScore);

    pos = TextIO::readFEN("8/8/K7/1P3p2/6k1/8/8/8 b - - 0 1");
    EXPECT_LT(std::abs(evalWhite(pos)), drawish);
    pos = TextIO::readFEN("8/8/K7/1P6/5pk1/8/8/8 b - - 0 1");
    EXPECT_LT(evalWhite(pos), -winScore);
    pos = TextIO::readFEN("8/K7/8/1P6/5pk1/8/8/8 b - - 0 1");
    EXPECT_LT(std::abs(evalWhite(pos)), drawish);
    pos = TextIO::readFEN("8/K7/8/8/1PP2p1k/8/8/8 w - - 0 1");
    EXPECT_LT(evalWhite(pos), drawish + pV);
    EXPECT_GT(evalWhite(pos), 0);
    pos = TextIO::readFEN("8/K7/8/8/1PP2p1k/8/8/8 b - - 0 1");
    EXPECT_LT(evalWhite(pos), -winScore + pV*3/2);
}

TEST(EvaluateTest, testKnightOutPost) {
    EvaluateTest::testKnightOutPost();
}

void
EvaluateTest::testKnightOutPost() {
    Position pos = TextIO::readFEN("rnrq2nk/ppp1p1pp/8/4Np2/3P4/8/P3P3/R1RQ2NK w KQkq - 0 1");
    int s1 = evalWhite(pos);
    pos = TextIO::readFEN("rnrq2nk/ppp1p1pp/8/3PNp2/8/8/P3P3/R1RQ2NK w KQkq - 0 1");
    int s2 = evalWhite(pos);
    EXPECT_LE(s2, s1 + 5);

    // Test knight fork bonus symmetry (currently no such term in the evaluation though)
    evalFEN("rnbqkb1r/ppp2Npp/3p4/8/2B1n3/8/PPPP1PPP/RNBQK2R b KQkq - 0 1");
    evalFEN("rnbqkb1r/ppN3pp/3p4/8/2B1n3/8/PPPP1PPP/RNBQK2R b KQkq - 0 1");
}


DECLARE_PARAM(testUciPar1, 60, 10, 80, true);
DECLARE_PARAM(testUciPar2, 120, 100, 300, true);

int uciParVec[3];

DEFINE_PARAM(testUciPar1);
DEFINE_PARAM(testUciPar2);

TEST(EvaluateTest, testUciParam) {
    EvaluateTest::testUciParam();
}

void
EvaluateTest::testUciParam() {
    testUciPar1.registerParam("uciPar1", Parameters::instance());
    testUciPar2.registerParam("uciPar2", Parameters::instance());

    testUciPar2.addListener([](){ uciParVec[0] = uciParVec[2] = testUciPar2; });

    EXPECT_EQ(60, static_cast<int>(testUciPar1));
    EXPECT_EQ(120, static_cast<int>(testUciPar2));
    EXPECT_EQ(120, uciParVec[0]);
    EXPECT_EQ(0, uciParVec[1]);
    EXPECT_EQ(120, uciParVec[2]);

    Parameters::instance().set("uciPar1", "70");
    EXPECT_EQ(70, static_cast<int>(testUciPar1));
    EXPECT_EQ(120, static_cast<int>(testUciPar2));
    EXPECT_EQ(120, uciParVec[0]);
    EXPECT_EQ(0, uciParVec[1]);
    EXPECT_EQ(120, uciParVec[2]);

    Parameters::instance().set("uciPar2", "180");
    EXPECT_EQ(70, static_cast<int>(testUciPar1));
    EXPECT_EQ(180, static_cast<int>(testUciPar2));
    EXPECT_EQ(180, uciParVec[0]);
    EXPECT_EQ(0, uciParVec[1]);
    EXPECT_EQ(180, uciParVec[2]);

    // Test button parameters
    int cnt1 = 0;
    int id1 = UciParams::clearHash->addListener([&cnt1]() {
        cnt1++;
    }, false);
    EXPECT_EQ(0, cnt1);
    Parameters::instance().set("Clear Hash", "");
    EXPECT_EQ(1, cnt1);
    Parameters::instance().set("Clear hash", "");
    EXPECT_EQ(2, cnt1);
    Parameters::instance().set("clear hash", "");
    EXPECT_EQ(3, cnt1);

    int cnt2 = 0;
    std::shared_ptr<Parameters::ButtonParam> testButton2(std::make_shared<Parameters::ButtonParam>("testButton2"));
    Parameters::instance().addPar(testButton2);
    int id2 = testButton2->addListener([&cnt2]() {
        cnt2++;
    }, false);
    EXPECT_EQ(3, cnt1);
    EXPECT_EQ(0, cnt2);
    Parameters::instance().set("testButton2", "");
    EXPECT_EQ(3, cnt1);
    EXPECT_EQ(1, cnt2);
    Parameters::instance().set("Clear Hash", "");
    EXPECT_EQ(4, cnt1);
    EXPECT_EQ(1, cnt2);

    UciParams::clearHash->removeListener(id1);
    Parameters::instance().set("Clear Hash", "");
    EXPECT_EQ(4, cnt1);
    EXPECT_EQ(1, cnt2);
    Parameters::instance().set("testButton2", "");
    EXPECT_EQ(4, cnt1);
    EXPECT_EQ(2, cnt2);

    testButton2->removeListener(id2);
    Parameters::instance().set("Clear Hash", "");
    EXPECT_EQ(4, cnt1);
    EXPECT_EQ(2, cnt2);
    Parameters::instance().set("testButton2", "");
    EXPECT_EQ(4, cnt1);
    EXPECT_EQ(2, cnt2);
}

ParamTable<10> uciParTable { 0, 100, true,
    { 0,2,3,5,-7,7,5,3,0,-2 },
    { 0,1,2,3,-4,4,3,2,0,-1 }
};

ParamTableMirrored<10> uciParTableM(uciParTable);

TEST(EvaluateTest, testUciParamTable) {
    EvaluateTest::testUciParamTable();
}

void
EvaluateTest::testUciParamTable() {
    EXPECT_EQ(0, uciParTable[0]);
    EXPECT_EQ(2, uciParTable[1]);
    EXPECT_EQ(3, uciParTable[2]);

    EXPECT_EQ(-2, uciParTableM[0]);
    EXPECT_EQ(0, uciParTableM[1]);
    EXPECT_EQ(3, uciParTableM[2]);
    EXPECT_EQ(0, uciParTableM[9]);
    EXPECT_EQ(2, uciParTableM[8]);
    EXPECT_EQ(3, uciParTableM[7]);

    uciParTable.registerParams("uciParTable", Parameters::instance());
    const int* table = uciParTable.getTable();
    const int* tableM = uciParTableM.getTable();

    Parameters::instance().set("uciParTable1", "11");
    {
        std::vector<int> expected { 0,11,3,5,-7,7,5,3,0,-11 };
        for (int i = 0; i < 10; i++) {
            EXPECT_EQ(expected[i], uciParTable[i]);
            EXPECT_EQ(expected[i], table[i]);
            EXPECT_EQ(expected[10-1-i], uciParTableM[i]);
            EXPECT_EQ(expected[10-1-i], tableM[i]);
        }
    }

    Parameters::instance().set("uciParTable2", "13");
    {
        std::vector<int> expected { 0,11,13,5,-7,7,5,13,0,-11 };
        for (int i = 0; i < 10; i++) {
            EXPECT_EQ(expected[i], uciParTable[i]);
            EXPECT_EQ(expected[i], table[i]);
            EXPECT_EQ(expected[10-1-i], uciParTableM[i]);
            EXPECT_EQ(expected[10-1-i], tableM[i]);
        }
    }

    Parameters::instance().set("uciParTable3", "17");
    {
        std::vector<int> expected { 0,11,13,17,-7,7,17,13,0,-11 };
        for (int i = 0; i < 10; i++) {
            EXPECT_EQ(expected[i], uciParTable[i]);
            EXPECT_EQ(expected[i], table[i]);
            EXPECT_EQ(expected[10-1-i], uciParTableM[i]);
            EXPECT_EQ(expected[10-1-i], tableM[i]);
        }
    }

    Parameters::instance().set("uciParTable4", "19");
    {
        std::vector<int> expected { 0,11,13,17,-19,19,17,13,0,-11 };
        for (int i = 0; i < 10; i++) {
            EXPECT_EQ(expected[i], uciParTable[i]);
            EXPECT_EQ(expected[i], table[i]);
            EXPECT_EQ(expected[10-1-i], uciParTableM[i]);
            EXPECT_EQ(expected[10-1-i], tableM[i]);
        }
    }
}

TEST(EvaluateTest, testSwindleScore) {
    EvaluateTest::testSwindleScore();
}

void
EvaluateTest::testSwindleScore() {
    for (int e = 0; e < 3000; e++) {
        int s1 = Evaluate::swindleScore(e, 0);
        ASSERT_GE(s1, (e?1:0));
        ASSERT_LT(s1, 50);
        ASSERT_LE(s1, e);
        ASSERT_LE(s1, Evaluate::swindleScore(e+1, 0));
        int s2 = Evaluate::swindleScore(-e, 0);
        ASSERT_EQ(-s1, s2);
    }

    for (int e = 0; e < 1000; e += 10) {
        for (int d = 1; d < 35; d++) {
            int s0 = Evaluate::swindleScore(e, 0);
            int s1 = Evaluate::swindleScore(e, d);
            int s2 = Evaluate::swindleScore(e, d+1);
            ASSERT_LE(0, s0);
            ASSERT_LT(s0, s2);
            ASSERT_LT(s2, s1);
        }
        for (int d = 1; d < 35; d++) {
            int s0 = Evaluate::swindleScore(-e, 0);
            int s1 = Evaluate::swindleScore(-e, -d);
            int s2 = Evaluate::swindleScore(-e, -(d+1));
            ASSERT_GE(0, s0);
            ASSERT_GT(s0, s2);
            ASSERT_GT(s2, s1);
        }
    }

    int s0 = Evaluate::swindleScore(5000, 0);
    int s1 = Evaluate::swindleScore(3, 1000);
    EXPECT_GT(s1, s0);

    s0 = Evaluate::swindleScore(-5000, 0);
    s1 = Evaluate::swindleScore(-3, -1000);
    EXPECT_LT(s1, s0);
}

TEST(EvaluateTest, testStalePawns) {
    EvaluateTest::testStalePawns();
}

void
EvaluateTest::testStalePawns() {
    Position pos = TextIO::readFEN("rnbqkbnr/3p1p1p/2p1p1p1/1p2P1P1/p2P1P2/2P5/PP5P/RNBQKBNR w KQkq - 0 1");
    U64 sp = Evaluate::computeStalePawns(pos);
    EXPECT_EQ(BitBoard::sqMask(A2,E5,E6,F4,F7,G5,G6), sp);

    pos = TextIO::readFEN("rnbqkbnr/3p1p1p/2p1p1p1/1p2P1P1/p2P1P2/P1P5/1P5P/RNBQKBNR b KQkq - 0 1");
    sp = Evaluate::computeStalePawns(pos);
    EXPECT_EQ(BitBoard::sqMask(A3,A4,B5,D4,E5,E6,F4,F7,G5,G6), sp);

    pos = TextIO::readFEN("rnbqkbnr/3ppp1p/2p3p1/1p2P1P1/p2P1P2/P1P5/1P5P/RNBQKBNR b KQkq - 0 1");
    sp = Evaluate::computeStalePawns(pos);
    EXPECT_EQ(BitBoard::sqMask(A3,A4,E5,E7,G5,G6), sp);

    pos = TextIO::readFEN("rnbqkbnr/3p1p1p/2p3p1/1p2P1P1/p2PpP2/P1P5/1P5P/RNBQKBNR b KQkq - 0 1");
    sp = Evaluate::computeStalePawns(pos);
    EXPECT_EQ(BitBoard::sqMask(A3,A4,B5,E4,E5,F7,G5,G6), sp);

    pos = TextIO::readFEN("1r2k2r/3bbp1p/2p1p1pP/p2p2P1/NP2PP2/P1P5/4B3/2KR2R1 b k - 0 1");
    sp = Evaluate::computeStalePawns(pos);
    EXPECT_EQ(BitBoard::sqMask(A3,F4,G5,G6,H6,H7), sp);

    pos = TextIO::readFEN("1r2k2r/3bbp1p/2p1p1pP/p2pP1P1/NP3P2/P1P5/4B3/2KR2R1 b k - 0 1");
    sp = Evaluate::computeStalePawns(pos);
    EXPECT_EQ(BitBoard::sqMask(A3,E5,E6,F4,F7,G5,G6,H6,H7), sp);

    pos = TextIO::readFEN("r3r1k1/pp1q1p2/2p2npb/PPPp1bnp/3PpN2/2N1P1PP/1R1B1PBK/3Q1R2 b - - 0 19");
    sp = Evaluate::computeStalePawns(pos);
    EXPECT_EQ(BitBoard::sqMask(C5,D4,D5,E3,E4,H3), sp);

    pos = TextIO::readFEN("6k1/5p2/4p3/3pP3/2pP4/2P5/8/6K1 w - -");
    sp = Evaluate::computeStalePawns(pos);
    EXPECT_EQ(BitBoard::sqMask(C3,C4,D4,D5,E5,E6), sp);

    pos = TextIO::readFEN("r1bq1rk1/ppp4n/3p2nB/P1PPp2p/1P2P3/1QNN1Pp1/4B1P1/R4RK1 w - - 1 3");
    sp = Evaluate::computeStalePawns(pos);
    EXPECT_EQ(BitBoard::sqMask(A7,D5,E4,E5,G2,G3), sp);

    pos = TextIO::readFEN("r1n1n1kr/p2bq1p1/Pp1p1pPp/1PpPpP1P/2P1P3/1B1N1QN1/6K1/R6R w - - 0 1");
    sp = Evaluate::computeStalePawns(pos);
    EXPECT_EQ(BitBoard::sqMask(A6,A7,B5,B6,C4,C5,D5,D6,E4,E5,F5,F6,G6,G7,H5,H6), sp);

    pos = TextIO::readFEN("r1bq1rk1/ppp1n1bp/3p1np1/3Pp3/1PP1Pp2/2N2P2/P2NB1PP/1RBQ1RK1 w - - 0 13");
    sp = Evaluate::computeStalePawns(pos);
    EXPECT_EQ(BitBoard::sqMask(D5,D6,E4,E5,F3,F4), sp);

    pos = TextIO::readFEN("1r3rk1/1q1bb1pp/2nn1p2/1pp1pP2/2PpP1P1/1P1P3P/4N1B1/R1BQ1RNK w - - 0 19");
    sp = Evaluate::computeStalePawns(pos);
    EXPECT_EQ(BitBoard::sqMask(C5,D3,D4,E4,E5,F5,F6), sp);

    pos = TextIO::readFEN("rn1q2rk/ppp1b3/3p1n2/3Pp1pp/1PB1PpP1/2N5/P1PBQPP1/1R3RK1 w - - 0 19");
    sp = Evaluate::computeStalePawns(pos);
    EXPECT_EQ(BitBoard::sqMask(B7,D5,D6,E4,E5,F2,G5), sp);

    pos = TextIO::readFEN("r2q1rk1/pb2bppp/1pn2n2/2p1pP2/2PpP3/3P4/PP2B1PP/RNBQNRK1 b - - 0 1");
    sp = Evaluate::computeStalePawns(pos);
    EXPECT_EQ(BitBoard::sqMask(C4,C5,D3,D4,E4,E5,F7), sp);
}

int
EvaluateTest::getNContactChecks(const std::string& fen) {
    Position pos = TextIO::readFEN(fen);
    Position symPos = swapColors(pos);
    std::string symFen = TextIO::toFEN(symPos);

    static std::shared_ptr<Evaluate::EvalHashTables> et;
    if (!et)
        et = Evaluate::getEvalHashTables();
    Evaluate eval(*et);
    evalPos(eval, pos, false, false);
    int nContact = eval.getNContactChecks(pos);

    evalPos(eval, symPos, false, false);
    int nContact2 = eval.getNContactChecks(symPos);
    EXPECT_EQ(-nContact, nContact2) << (fen + " == " + symFen);

    return nContact;
}

TEST(EvaluateTest, testContactChecks) {
    EvaluateTest::testContactChecks();
}

void
EvaluateTest::testContactChecks() {
    EXPECT_EQ(0, getNContactChecks(TextIO::startPosFEN));
    EXPECT_EQ(1, getNContactChecks("r1bqkbnr/pppp1ppp/2n5/4p2Q/2B1P3/8/PPPP1PPP/RNB1K1NR w KQkq - 0 1"));
    EXPECT_EQ(0, getNContactChecks("r1bqkb1r/pppp1ppp/2n4n/4p2Q/2B1P3/8/PPPP1PPP/RNB1K1NR w KQkq - 0 1"));
    EXPECT_EQ(0, getNContactChecks("r1b1kbnr/ppppqppp/2n5/4p2Q/2B1P3/8/PPPP1PPP/RNB1K1NR w KQkq - 0 1"));
    EXPECT_EQ(0, getNContactChecks("r1b1kbnr/ppppqppp/2n5/4p2Q/2B1P3/5R2/PPPP1PPP/RNB1K1N1 w Qkq - 0 1")); //
    EXPECT_EQ(0, getNContactChecks("2b1kbnr/ppprqppp/2n5/4p2Q/2B1P3/5R2/PPPP1PPP/RNB1K1N1 w Qk - 0 1"));
    EXPECT_EQ(1, getNContactChecks("r2q1rk1/pbppppbp/1pn3p1/6N1/3PP2Q/2N5/PPPB1PP1/2KRR3 w - - 0 1"));
    EXPECT_EQ(2, getNContactChecks("r2q1rk1/pbpppp2/1pn3pQ/8/4P3/2BP1N2/PPP1NPP1/2KRR3 w - - 0 1"));
    EXPECT_EQ(1, getNContactChecks("r4rk1/pbpppp2/1p4pQ/8/nq1BP3/2NP1N2/PPP2PP1/2KRR3 w - - 0 1"));
    EXPECT_EQ(2, getNContactChecks("rnbq1rk1/pppppp1p/5PpQ/6N1/8/8/PPPPP1PP/RNB1KB1R w KQ - 0 1"));
    EXPECT_EQ(2, getNContactChecks("rnbq1rk1/ppppp3/6K1/4Q3/8/5N2/PPPPP1P1/RNB2B1R w - - 0 1"));
    EXPECT_EQ(0, getNContactChecks("r1b1qr2/pp2npp1/1b2p2k/nP1pP1NP/6Q1/2P5/P4PP1/RNB1K2R b KQ - 2 14"));
}
