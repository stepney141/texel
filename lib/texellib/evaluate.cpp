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
 * evaluate.cpp
 *
 *  Created on: Feb 25, 2012
 *      Author: petero
 */

#include "evaluate.hpp"
#include "endGameEval.hpp"
#include "constants.hpp"
#include "parameters.hpp"
#include "chessError.hpp"
#include "incbin.h"
#include <vector>

extern "C" {
#include "Lzma86Dec.h"
}

int Evaluate::pieceValueOrder[Piece::nPieceTypes] = {
    0,
    5, 4, 3, 2, 2, 1,
    5, 4, 3, 2, 2, 1
};


static const int empty[64] = { 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
                               0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
                               0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
                               0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0};

static StaticInitializer<Evaluate> evInit;

INCBIN_EXTERN(char, NNData);
// const char* gNNDataData;
// const unsigned int gNNDataSize;

void
Evaluate::staticInitialize() {
    psTab1[Piece::EMPTY]   = empty;
    psTab1[Piece::WKING]   = kt1w.getTable();
    psTab1[Piece::WQUEEN]  = qt1w.getTable();
    psTab1[Piece::WROOK]   = rt1w.getTable();
    psTab1[Piece::WBISHOP] = bt1w.getTable();
    psTab1[Piece::WKNIGHT] = nt1w.getTable();
    psTab1[Piece::WPAWN]   = pt1w.getTable();
    psTab1[Piece::BKING]   = kt1b.getTable();
    psTab1[Piece::BQUEEN]  = qt1b.getTable();
    psTab1[Piece::BROOK]   = rt1b.getTable();
    psTab1[Piece::BBISHOP] = bt1b.getTable();
    psTab1[Piece::BKNIGHT] = nt1b.getTable();
    psTab1[Piece::BPAWN]   = pt1b.getTable();

    psTab2[Piece::EMPTY]   = empty;
    psTab2[Piece::WKING]   = kt2w.getTable();
    psTab2[Piece::WQUEEN]  = qt2w.getTable();
    psTab2[Piece::WROOK]   = rt1w.getTable();
    psTab2[Piece::WBISHOP] = bt2w.getTable();
    psTab2[Piece::WKNIGHT] = nt2w.getTable();
    psTab2[Piece::WPAWN]   = pt2w.getTable();
    psTab2[Piece::BKING]   = kt2b.getTable();
    psTab2[Piece::BQUEEN]  = qt2b.getTable();
    psTab2[Piece::BROOK]   = rt1b.getTable();
    psTab2[Piece::BBISHOP] = bt2b.getTable();
    psTab2[Piece::BKNIGHT] = nt2b.getTable();
    psTab2[Piece::BPAWN]   = pt2b.getTable();
}

const int* Evaluate::psTab1[Piece::nPieceTypes];
const int* Evaluate::psTab2[Piece::nPieceTypes];

Evaluate::Evaluate(EvalHashTables& et)
    : pawnHash(et.pawnHash),
      materialHash(et.materialHash),
      evalHash(et.evalHash),
      nnEval(et.nnEval),
      whiteContempt(0) {
}

int
Evaluate::evalPos(const Position& pos) {
    return evalPos<false>(pos);
}

int
Evaluate::evalPosPrint(const Position& pos) {
    return evalPos<true>(pos);
}

template <bool print>
inline int
Evaluate::evalPos(const Position& pos) {
    nnEval.setPos(pos);
    return nnEval.eval();

    const bool useHashTable = !print;
    EvalHashData* ehd = nullptr;
    U64 key = pos.historyHash();
    if (useHashTable) {
        ehd = &getEvalHashEntry(key);
        if ((ehd->data ^ key) < (1 << 16))
            return (ehd->data & 0xffff) - (1 << 15);
    }

    int score = 0;
    score += materialScore(pos, print);

    score += pieceSquareEval(pos);
    if (print) std::cout << "info string eval pst    :" << score << std::endl;
    pawnBonus(pos);

    if (mhd->endGame)
        score = EndGameEval::endGameEval<true>(pos, score);
    if (print) std::cout << "info string eval endgame:" << score << std::endl;

    if ((whiteContempt != 0) && !mhd->endGame) {
        int mtrlPawns = pos.wMtrlPawns() + pos.bMtrlPawns();
        int mtrl = pos.wMtrl() + pos.bMtrl();
        int hiMtrl = (rV + bV*2 + nV*2) * 2;
        int piecePlay = interpolate(mtrl - mtrlPawns, 0, 64, hiMtrl, 128);
        score += whiteContempt * piecePlay / 128;
        if (print) std::cout << "info string eval contemp:" << score << ' ' << piecePlay << std::endl;
    }

    if (pos.pieceTypeBB(Piece::WPAWN, Piece::BPAWN)) {
        int hmc = clamp(pos.getHalfMoveClock() / 10, 0, 9);
        score = score * halfMoveFactor[hmc] / 128;
    }
    if (print) std::cout << "info string eval halfmove:" << score << std::endl;

    if (score > 0) {
        int nStale = BitBoard::bitCount(BitBoard::southFill(phd->stalePawns & pos.pieceTypeBB(Piece::WPAWN)) & 0xff);
        score = score * stalePawnFactor[nStale] / 128;
    } else if (score < 0) {
        int nStale = BitBoard::bitCount(BitBoard::southFill(phd->stalePawns & pos.pieceTypeBB(Piece::BPAWN)) & 0xff);
        score = score * stalePawnFactor[nStale] / 128;
    }
    if (print) std::cout << "info string eval staleP :" << score << std::endl;

    if (!pos.isWhiteMove())
        score = -score;

    if (useHashTable)
        ehd->data = (key & 0xffffffffffff0000ULL) + (score + (1 << 15));

    return score;
}

/** Compensate for the fact that many knights are stronger compared to queens
 * than what the default material scores would predict. */
static inline int correctionNvsQ(int n, int q) {
    if (n <= q+1)
        return 0;
    int knightBonus = 0;
    if (q == 1)
        knightBonus = knightVsQueenBonus1;
    else if (q == 2)
        knightBonus = knightVsQueenBonus2;
    else if (q >= 3)
        knightBonus = knightVsQueenBonus3;
    int corr = knightBonus * (n - q - 1);
    return corr;
}

void
Evaluate::computeMaterialScore(const Position& pos, MaterialHashData& mhd, bool print) const {
    int score = 0;

    const int nWQ = BitBoard::bitCount(pos.pieceTypeBB(Piece::WQUEEN));
    const int nBQ = BitBoard::bitCount(pos.pieceTypeBB(Piece::BQUEEN));
    const int nWN = BitBoard::bitCount(pos.pieceTypeBB(Piece::WKNIGHT));
    const int nBN = BitBoard::bitCount(pos.pieceTypeBB(Piece::BKNIGHT));
    int wCorr = correctionNvsQ(nWN, nBQ);
    int bCorr = correctionNvsQ(nBN, nWQ);
    score += wCorr - bCorr;
    if (print) std::cout << "info string eval qncorr :" << score << std::endl;

    mhd.id = pos.materialId();
    mhd.score = score;
    mhd.endGame = EndGameEval::endGameEval<false>(pos, 0);
}

int
Evaluate::pieceSquareEval(const Position& pos) {
    int score = 0;

    // Kings/pawns
    if (pos.wMtrlPawns() + pos.bMtrlPawns() == 0) { // Use symmetric tables if no pawns left
        if (pos.wMtrl() > pos.bMtrl())
            score += EndGameEval::mateEval(pos.getKingSq(true), pos.getKingSq(false));
        else if (pos.wMtrl() < pos.bMtrl())
            score -= EndGameEval::mateEval(pos.getKingSq(false), pos.getKingSq(true));
        else
            score += EndGameEval::winKingTable[pos.getKingSq(true)] -
                     EndGameEval::winKingTable[pos.getKingSq(false)];
    }

    return score;
}

void
Evaluate::pawnBonus(const Position& pos) {
    U64 key = pos.pawnZobristHash();
    PawnHashData& phd = getPawnHashEntry(key);
    if (phd.key != key)
        computePawnHashData(pos, phd);
    this->phd = &phd;
}

/** Compute subset of squares given by mask that white is in control over, ie
 *  squares that have at least as many white pawn guards as black has pawn
 *  attacks on the square. */
static inline U64
wPawnCtrlSquares(U64 mask, U64 wPawns, U64 bPawns) {
    U64 wLAtks = (wPawns & BitBoard::maskBToHFiles) << 7;
    U64 wRAtks = (wPawns & BitBoard::maskAToGFiles) << 9;
    U64 bLAtks = (bPawns & BitBoard::maskBToHFiles) >> 9;
    U64 bRAtks = (bPawns & BitBoard::maskAToGFiles) >> 7;
    return ((mask & ~bLAtks & ~bRAtks) |
            (mask & (bLAtks ^ bRAtks) & (wLAtks | wRAtks)) |
            (mask & wLAtks & wRAtks));
}

static inline U64
bPawnCtrlSquares(U64 mask, U64 wPawns, U64 bPawns) {
    U64 wLAtks = (wPawns & BitBoard::maskBToHFiles) << 7;
    U64 wRAtks = (wPawns & BitBoard::maskAToGFiles) << 9;
    U64 bLAtks = (bPawns & BitBoard::maskBToHFiles) >> 9;
    U64 bRAtks = (bPawns & BitBoard::maskAToGFiles) >> 7;
    return ((mask & ~wLAtks & ~wRAtks) |
            (mask & (wLAtks ^ wRAtks) & (bLAtks | bRAtks)) |
            (mask & bLAtks & bRAtks));
}

U64
Evaluate::computeStalePawns(const Position& pos) {
    const U64 wPawns = pos.pieceTypeBB(Piece::WPAWN);
    const U64 bPawns = pos.pieceTypeBB(Piece::BPAWN);

    // Compute stale white pawns
    U64 wStale;
    {
        U64 wPawnCtrl = wPawnCtrlSquares(wPawns, wPawns, bPawns);
        for (int i = 0; i < 4; i++)
            wPawnCtrl |= wPawnCtrlSquares((wPawnCtrl << 8) & ~bPawns, wPawnCtrl, bPawns);
        wPawnCtrl &= ~BitBoard::maskRow8;
        U64 wPawnCtrlLAtk = (wPawnCtrl & BitBoard::maskBToHFiles) << 7;
        U64 wPawnCtrlRAtk = (wPawnCtrl & BitBoard::maskAToGFiles) << 9;

        U64 bLAtks = (bPawns & BitBoard::maskBToHFiles) >> 9;
        U64 bRAtks = (bPawns & BitBoard::maskAToGFiles) >> 7;
        U64 wActive = ((bLAtks ^ bRAtks) |
                       (bLAtks & bRAtks & (wPawnCtrlLAtk | wPawnCtrlRAtk)));
        for (int i = 0; i < 4; i++)
            wActive |= (wActive & ~(wPawns | bPawns)) >> 8;
        wStale = wPawns & ~wActive;
    }

    // Compute stale black pawns
    U64 bStale;
    {
        U64 bPawnCtrl = bPawnCtrlSquares(bPawns, wPawns, bPawns);
        for (int i = 0; i < 4; i++)
            bPawnCtrl |= bPawnCtrlSquares((bPawnCtrl >> 8) & ~wPawns, wPawns, bPawnCtrl);
        bPawnCtrl &= ~BitBoard::maskRow1;
        U64 bPawnCtrlLAtk = (bPawnCtrl & BitBoard::maskBToHFiles) >> 9;
        U64 bPawnCtrlRAtk = (bPawnCtrl & BitBoard::maskAToGFiles) >> 7;

        U64 wLAtks = (wPawns & BitBoard::maskBToHFiles) << 7;
        U64 wRAtks = (wPawns & BitBoard::maskAToGFiles) << 9;
        U64 bActive = ((wLAtks ^ wRAtks) |
                       (wLAtks & wRAtks & (bPawnCtrlLAtk | bPawnCtrlRAtk)));
        for (int i = 0; i < 4; i++)
            bActive |= (bActive & ~(wPawns | bPawns)) << 8;
        bStale = bPawns & ~bActive;
    }

    return wStale | bStale;
}

void
Evaluate::computePawnHashData(const Position& pos, PawnHashData& ph) {
    const U64 wPawns = pos.pieceTypeBB(Piece::WPAWN);
    const U64 bPawns = pos.pieceTypeBB(Piece::BPAWN);
    U64 wPawnAttacks = BitBoard::wPawnAttacksMask(pos.pieceTypeBB(Piece::WPAWN));
    U64 bPawnAttacks = BitBoard::bPawnAttacksMask(pos.pieceTypeBB(Piece::BPAWN));
    U64 passedPawnsW = wPawns & ~BitBoard::southFill(bPawns | bPawnAttacks | (wPawns >> 8));
    U64 passedPawnsB = bPawns & ~BitBoard::northFill(wPawns | wPawnAttacks | (bPawns << 8));
    U64 stalePawns = computeStalePawns(pos) & ~passedPawnsW & ~passedPawnsB;

    ph.key = pos.pawnZobristHash();
    ph.stalePawns = stalePawns;
}

std::unique_ptr<Evaluate::EvalHashTables>
Evaluate::getEvalHashTables() {
    return make_unique<EvalHashTables>();
}

Evaluate::EvalHashTables::EvalHashTables()
    : nnEval(initNetData()) {
    pawnHash.resize(1 << 16);
    materialHash.resize(1 << 14);
}

NetData&
Evaluate::EvalHashTables::initNetData() {
    netData = NetData::create();

    size_t unCompressedSize = netData->computeSize();
    std::vector<unsigned char> unComprData(unCompressedSize);
    unsigned char* compressedData = (unsigned char*)gNNDataData;
    size_t compressedSize = gNNDataSize;
    int res = Lzma86_Decode(unComprData.data(), &unCompressedSize, compressedData, &compressedSize);
    if (res != SZ_OK)
        throw ChessError("Failed to decompress network data");

    std::string nnData((char*)unComprData.data(), unCompressedSize);
    std::stringstream is(nnData);
    netData->load(is);
    return *netData;
}

int
Evaluate::swindleScore(int evalScore, int distToWin) {
    using namespace SearchConst;
    if (distToWin == 0) {
        int sgn = evalScore >= 0 ? 1 : -1;
        int score = std::abs(evalScore) + 4;
        int lg = BitUtil::lastBit(score);
        score = (lg - 3) * 4 + (score >> (lg - 2));
        score = std::min(score, minFrustrated - 1);
        return sgn * score;
    } else {
        int sgn = distToWin > 0 ? 1 : -1;
        return sgn * std::max(maxFrustrated + 1 - std::abs(distToWin), minFrustrated);
    }
}
