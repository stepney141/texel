/*
    Texel - A UCI chess engine.
    Copyright (C) 2021  Peter Österlund, peterosterlund2@gmail.com

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
 * proofkernel.hpp
 *
 *  Created on: Oct 16, 2021
 *      Author: petero
 */

#ifndef PROOFKERNEL_HPP_
#define PROOFKERNEL_HPP_

#include "util/util.hpp"
#include <array>

class Position;


/** The ProofKerrnel class is used for finding a sequence of captures and promotions
 *  that transform the material configuration of a starting position to the material
 *  configuration of a goal position.
 *  The lack of a proof kernel means the corresponding proof game problem has no solution.
 *  The existence of a proof kernel does not mean a corresponding proof game exists, but
 *  hopefully most of the time a proof game does exist in this case. */
class ProofKernel {
public:
    /** Constructor. */
    ProofKernel(const Position& initialPos, const Position& goalPos);

    enum PieceColor {
        WHITE,
        BLACK,
    };

    enum PieceType {
        QUEEN,
        ROOK,
        DARK_BISHOP,
        LIGHT_BISHOP,
        KNIGHT,
        PAWN,
        EMPTY,
    };

    /** Represents a move in the proof kernel state space. Each move reduces the
     *  total number of pieces by one. Possible moves are of the following types:
     *  Move type                Example
     *  pawn takes pawn          wPc0xPb1  First c pawn takes second pawn on b file
     *  pawn takes piece         wPc0xRb0  First c pawn takes rook on b file. Pawn placed at index 0 in b file
     *                           wPc0xRbQ  First c pawn takes rook on b file and promotes to queen
     *  pawn takes promoted pawn wPc0xfb0  First c pawn takes piece on b file coming from promotion on f file
     *  piece takes pawn         bxPc0     Black piece takes first pawn on c file
     *  piece takes piece        bxR       Black piece takes white rook */
    struct PkMove {
        PieceColor color;        // Color of moving piece
        int fromFile;            // File of moving pawn, or -1 if not pawn move
        int fromIdx;             // Index in pawn column, or -1 if not pawn move

        PieceType takenPiece;    // Cannot be EMPTY. Always set to KNIGHT if promoted piece taken
        int otherPromotionFile;  // File where other pawn promoted, or -1

        int toFile;              // File of taken piece
        int toIdx;               // Index in pawn column. Insertion index if takenPiece != PAWN. -1 if promotion
        PieceType promotedPiece; // Promoted piece, or EMPTY
    };

    /** Computes a proof kernel, as a sequence of PkMoves, for the given initial and goal positions.
     *  A proof kernel, when applied to the initial position, results in a position that has the
     *  same number of white and black pieces as the goal position, and where promotions can
     *  be performed to get the same number of pieces as the goal position for each piece type.
     *  @param result  Computed if true is returned.
     *  @return        True if a proof kernel exists, false otherwise. */
    bool findProofKernel(std::vector<PkMove>& result);

private:
    enum class SquareColor { // Square color, important for bishops
        DARK,
        LIGHT,
    };
    enum class Direction { // Possible pawn move directions
        LEFT,
        FORWARD,
        RIGHT,
    };

    /** Represents all pawns (0 - 6) on a file.*/
    class PawnColumn {
    public:
        /** Number of pawns in the column. */
        int nPawns() const;
        /** Get color of the i:th pawn. 0 <= i < nPawns(). */
        PieceColor getPawn(int i) const;

        /** Insert a pawn at position "i". 0 <= i <= nPawns(). */
        void addPawn(int i, PieceColor c);
        /** Remove the i:th pawn. 0 <= i < nPawns(). */
        void removePawn(int i);
        /** Sets the i:th pawn to color "c". 0 <= i < nPawns(). */
        void setPawn(int i, PieceColor c);

        // State that does not change during search
        /** True if a pawn can promote in a given direction from this file. */
        bool canPromote(PieceColor c, Direction d) const;
        /** True if promotion to rook/queen is possible. */
        bool rookQueenPromotePossible(PieceColor c) const;
        /** Color of promotion square. */
        SquareColor promotionSquareType(PieceColor c) const;
    private:
        U8 data;
        SquareColor promSquare[2]; // Color of promotion square for white/black
    };
    std::array<PawnColumn, 8> columns;
    static const int nPieceTypes = EMPTY;
    int pieceCnt[2][nPieceTypes];
    int goalCnt[2][nPieceTypes];
    int excessCnt[2][nPieceTypes];  // pieceCnt - goalCnt
};

#endif /* PROOFKERNEL_HPP_ */
