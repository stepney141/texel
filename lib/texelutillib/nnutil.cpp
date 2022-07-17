/*
    Texel - A UCI chess engine.
    Copyright (C) 2022  Peter Österlund, peterosterlund2@gmail.com

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
 * nnutil.cpp
 *
 *  Created on: Jul 14, 2022
 *      Author: petero
 */

#include "nnutil.hpp"
#include "position.hpp"
#include "posutil.hpp"

static Piece::Type ptVec[] = { Piece::WQUEEN, Piece::WROOK, Piece::WBISHOP, Piece::WKNIGHT, Piece::WPAWN,
                               Piece::BQUEEN, Piece::BROOK, Piece::BBISHOP, Piece::BKNIGHT, Piece::BPAWN};

void
NNUtil::posToRecord(Position& pos, int searchScore, Record& r) {
    r.searchScore = searchScore;

    if (!pos.isWhiteMove()) {
        pos = PosUtil::swapColors(pos);
        r.searchScore *= -1;
    }

    r.wKing = pos.getKingSq(true);
    r.bKing = pos.getKingSq(false);
    r.halfMoveClock = pos.getHalfMoveClock();

    int p = 0;
    int i = 0;
    for (Piece::Type pt : ptVec) {
        U64 mask = pos.pieceTypeBB(pt);
        while (mask) {
            int sq = BitBoard::extractSquare(mask);
            r.squares[i++] = sq;
        }
        if (p < 9)
            r.nPieces[p++] = i;
    }
    while (i < 30)
        r.squares[i++] = -1;
}

void
NNUtil::recordToPos(const Record& r, Position& pos, int& searchScore) {
    for (int sq = 0; sq < 64; sq++)
        pos.clearPiece(sq);

    pos.setPiece(r.wKing, Piece::WKING);
    pos.setPiece(r.bKing, Piece::BKING);

    int pieceType = 0;
    for (int i = 0; i < 30; i++) {
        while (pieceType < 9 && i >= r.nPieces[pieceType])
            pieceType++;
        int sq = r.squares[i];
        if (sq == -1)
            continue;
        pos.setPiece(sq, ptVec[pieceType]);
    }

    pos.setWhiteMove(true);
    pos.setCastleMask(0);
    pos.setEpSquare(-1);
    pos.setHalfMoveClock(r.halfMoveClock);
    pos.setFullMoveCounter(1);

    searchScore = r.searchScore;
}
