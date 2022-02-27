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
 * pkseq.cpp
 *
 *  Created on: Feb 5, 2022
 *      Author: petero
 */

#include "pkseq.hpp"
#include "proofgame.hpp"
#include "stloutput.hpp"
#include <cassert>
#include <limits.h>

using PieceColor = ProofKernel::PieceColor;
using PieceType = ProofKernel::PieceType;

PkSequence::PkSequence(const std::vector<ExtPkMove>& extKernel,
                       const Position& initPos, const Position& goalPos,
                       std::ostream& log)
    : extKernel(extKernel), initPos(initPos), goalPos(goalPos), log(log) {
}

void
PkSequence::improve() {
    if (extKernel.empty())
        return;

    splitPawnMoves();


    Graph kernel;
    for (const ExtPkMove& m : extKernel) {
        kernel.addNode(m);
    }

    Position pos(initPos);
    if (improveKernel(kernel, 0, pos)) {
        extKernel.clear();
        for (const MoveData& md : kernel.nodes)
            extKernel.push_back(md.move);
    }


    combinePawnMoves();
}

static bool
isNonCapturePawnMove(const ProofKernel::ExtPkMove& m) {
    if (m.movingPiece == PieceType::PAWN &&
        Square::getX(m.fromSquare) == Square::getX(m.toSquare)) {
        assert(!m.capture);
        return true;
    }
    return false;
}

void
PkSequence::splitPawnMoves() {
    std::vector<ExtPkMove> seq;
    for (const ExtPkMove& m : extKernel) {
        if (isNonCapturePawnMove(m)) {
            int x = Square::getX(m.fromSquare);
            int y1 = Square::getY(m.fromSquare);
            int y2 = Square::getY(m.toSquare);
            int d = y1 < y2 ? 1 : -1;
            for (int y = y1 + d; y != y2 + d; y += d) {
                ExtPkMove tmpM(m);
                tmpM.fromSquare = Square::getSquare(x, y1);
                tmpM.toSquare = Square::getSquare(x, y);
                if (y != y2)
                    tmpM.promotedPiece = PieceType::EMPTY;
                seq.push_back(tmpM);
                y1 = y;
            }
        } else {
            seq.push_back(m);
        }
    }
    extKernel = seq;
}

void
PkSequence::combinePawnMoves() {
    std::vector<ExtPkMove> seq;
    for (const ExtPkMove& m : extKernel) {
        bool merged = false;
        if (!seq.empty() && isNonCapturePawnMove(m) && isNonCapturePawnMove(seq.back())) {
            const ExtPkMove& m0 = seq.back();
            int x = Square::getX(m.fromSquare);
            if (x == Square::getX(m0.fromSquare) &&
                Square::getY(m0.toSquare) == Square::getY(m.fromSquare)) {
                int y0 = Square::getY(m0.fromSquare);
                int y1 = Square::getY(m.toSquare);
                bool white = (m.color == PieceColor::WHITE);
                if (y0 == (white ? 1 : 6) && y1 == (white ? 3 : 4)) {
                    seq.back() = m;
                    seq.back().fromSquare = Square::getSquare(x, y0);
                    merged = true;
                }
            }
        }
        if (!merged)
            seq.push_back(m);
    }
    extKernel = seq;
}

bool
PkSequence::improveKernel(Graph& kernel, int idx, const Position& pos) const {
    if (idx >= (int)kernel.nodes.size())
        return true;

    MoveData& md = kernel.nodes[idx];
    ExtPkMove& m = md.move;

    if (m.movingPiece == PieceType::PAWN) {
        UndoInfo ui;
        Position tmpPos(pos);
        if (!makeMove(tmpPos, ui, m))
            return false;
        return improveKernel(kernel, idx + 1, tmpPos);
    }

    if (!md.pseudoLegal) {
        if (m.movingPiece == PieceType::EMPTY) {
            assert(m.capture);
            if (!assignPiece(kernel, idx, pos))
                return false;
        }

        // Try moving piece while respecting occupied squares
        {
            Graph tmpKernel(kernel);
            U64 blocked = pos.occupiedBB() & ~(1ULL << m.toSquare) & ~(1ULL << m.fromSquare);
            std::vector<ExtPkMove> expanded;
            if (expandPieceMove(m, blocked, expanded)) {
                tmpKernel.replaceNode(idx, expanded);
                if (improveKernel(tmpKernel, idx, pos)) {
                    kernel = tmpKernel;
                    return true;
                }
            }
        }

        auto updatePos = [](const Graph& tmpKernel, int idx, int id,
                            Position& pos) -> bool {
            UndoInfo ui;
            for (int i = idx; i < (int)tmpKernel.nodes.size(); i++) {
                if (tmpKernel.nodes[i].id == id)
                    break;
                if (!makeMove(pos, ui, tmpKernel.nodes[i].move)) {
                    return false;
                }
            }
            return true;
        };

        // Try moving later pawn moves earlier to make the piece movement possible
        for (int i = idx + 1; i < (int)kernel.nodes.size(); i++) {
            const MoveData& nextMd = kernel.nodes[i];
            const ExtPkMove& em = nextMd.move;
            if (em.movingPiece != PieceType::PAWN || em.promotedPiece != PieceType::EMPTY)
                continue;

            Graph tmpKernel(kernel);
            tmpKernel.nodes[idx].dependsOn.push_back(tmpKernel.nodes[i].id);
            if (!tmpKernel.topoSort())
                continue;

            Position tmpPos(pos);
            if (!updatePos(tmpKernel, idx, md.id, tmpPos))
                continue;

            U64 blocked = tmpPos.occupiedBB() & ~(1ULL << m.toSquare) & ~(1ULL << m.fromSquare);
            std::vector<ExtPkMove> expanded;
            if (expandPieceMove(m, blocked, expanded)) {
                if (!improveKernel(tmpKernel, idx, pos))
                    return false;
                kernel = tmpKernel;
                return true;
            }
        }

        // Try adding a pawn move to make piece movement possible
        std::vector<ExtPkMove> pawnMoves;
        getPawnMoves(kernel, idx, pos, pawnMoves);
        for (const ExtPkMove& pawnMove : pawnMoves) {
            Graph tmpKernel(kernel);
            tmpKernel.addNode(pawnMove);
            tmpKernel.nodes[idx].dependsOn.push_back(tmpKernel.nodes.back().id);
            if (!tmpKernel.topoSort())
                continue;

            Position tmpPos(pos);
            if (!updatePos(tmpKernel, idx, md.id, tmpPos))
                continue;

            U64 blocked = tmpPos.occupiedBB() & ~(1ULL << m.toSquare) & ~(1ULL << m.fromSquare);
            std::vector<ExtPkMove> expanded;
            if (expandPieceMove(m, blocked, expanded)) {
                if (!improveKernel(tmpKernel, idx, pos))
                    return false;
                kernel = tmpKernel;
                return true;
            }
        }

        return false;
    }

    if (md.pseudoLegal) {
        Position tmpPos(pos);
        UndoInfo ui;
        if (!makeMove(tmpPos, ui, m))
            return false;
        return improveKernel(kernel, idx + 1, tmpPos);
    }

    // After each move, test if the king is in check.
    // - If so, generate king evasion move.
    // - If king has nowhere to go, generate move to make space for the king
    //   before the checking move.

    // At end of proof kernel position, if some piece cannot reach its target
    // position, add an ExtPkMove for this piece movement. Then:
    // - Try to add a pawn move before the piece move to make the piece move
    //   possible.
    //   - If this does not work, try to move the piece move earlier in the move
    //      sequence where whatever was blocking its path is not yet in a
    //      blocking position.

    // If a rook cannot reach its target square, check if the other rook can
    // reach it instead.
    // - If so, swap which rook is moved.
    // - If there later is a move of the other rook, that move must be swapped
    //   too.

    // For a non-pawn move, decide if it is possible to make the move without
    // moving any other pieces.
    // - If other non-pawn piece needs to move, insert ExtPkMove to move it.
    //   - If target square conflicts with later move, also generate ExtPkMove
    //     to move the piece back to where it was.

    return false;
}

bool
PkSequence::makeMove(Position& pos, UndoInfo& ui, const ExtPkMove& move) {
    bool white = move.color == PieceColor::WHITE;

    int p = pos.getPiece(move.toSquare);
    if (move.capture) {
        if (p == Piece::EMPTY || Piece::isWhite(p) == white)
            return false; // Wrong capture piece
    } else {
        if (p != Piece::EMPTY)
            return false; // Target square occupied
    }

    if (move.movingPiece == PieceType::EMPTY)
        return false;

    int promoteTo = Piece::EMPTY;
    if (move.promotedPiece != PieceType::EMPTY)
        promoteTo = ProofKernel::toPieceType(white, move.promotedPiece, false);
    Move m(move.fromSquare, move.toSquare, promoteTo);
    pos.makeMove(m, ui);
    pos.setWhiteMove(true);

    return true;
}

bool
PkSequence::assignPiece(Graph& kernel, int idx, const Position& pos) const {
    ExtPkMove& move = kernel.nodes[idx].move;
    bool whiteMoving = !Piece::isWhite(pos.getPiece(move.toSquare));
    U64 candidates = whiteMoving ? pos.whiteBB() : pos.blackBB();
    candidates &= ~pos.pieceTypeBB(whiteMoving ? Piece::WPAWN : Piece::BPAWN);
    candidates &= ~pos.pieceTypeBB(whiteMoving ? Piece::WKING : Piece::BKING);
    if (pos.a1Castle()) candidates &= ~(1ULL << A1);
    if (pos.h1Castle()) candidates &= ~(1ULL << H1);
    if (pos.a8Castle()) candidates &= ~(1ULL << A8);
    if (pos.h8Castle()) candidates &= ~(1ULL << H8);

    int bestDist = INT_MAX;
    ProofGame::ShortestPathData spd;
    while (candidates != 0) {
        int sq = BitBoard::extractSquare(candidates);
        Piece::Type p = (Piece::Type)pos.getPiece(sq);

        U64 blocked = pos.occupiedBB() & ~(1ULL << sq) & ~(1ULL << move.toSquare);
        ProofGame::shortestPaths(p, move.toSquare, blocked, nullptr, spd);
        int dist = spd.pathLen[sq];
        if (dist > 0 && dist < bestDist) {
            move.movingPiece = ProofKernel::toPieceType(p, sq);
            move.fromSquare = sq;
            bestDist = dist;
        }
    }

    if (bestDist == INT_MAX)
        return false;

    // Update next move of the same piece
    for (int i = idx + 1; i < (int)kernel.nodes.size(); i++) {
        ExtPkMove& m = kernel.nodes[i].move;
        if (m.color == move.color &&
            m.movingPiece == move.movingPiece &&
            m.fromSquare == move.fromSquare) {
            m.fromSquare = move.toSquare;
            break;
        }
    }
    return true;
}

bool
PkSequence::expandPieceMove(const ExtPkMove& move, U64 blocked,
                            std::vector<ExtPkMove>& outMoves) {
    bool white = move.color == PieceColor::WHITE;
    Piece::Type p = ProofKernel::toPieceType(white, move.movingPiece, false);

    ProofGame::ShortestPathData spd;
    ProofGame::shortestPaths(p, move.toSquare, blocked, nullptr, spd);
    if (spd.pathLen[move.fromSquare] < 0)
        return false;

    int fromSq = move.fromSquare;
    int toSq = move.toSquare;
    while (fromSq != toSq) {
        U64 nextMask = spd.getNextSquares(p, fromSq, blocked);
        assert(nextMask != 0);
        int nextSq = BitBoard::firstSquare(nextMask);

        ExtPkMove m(move);
        m.fromSquare = fromSq;
        m.toSquare = nextSq;
        if (nextSq != toSq)
            m.capture = false;
        outMoves.push_back(m);

        fromSq = nextSq;
    }

    return true;
}

void
PkSequence::getPawnMoves(const Graph& kernel, int idx, const Position& inPos,
                         std::vector<ExtPkMove>& pawnMoves) const {
    // Remove all but pawns and kings
    Position tmpPos(inPos);
    for (int sq = 0; sq < 64; sq++) {
        int p = tmpPos.getPiece(sq);
        switch (p) {
        case Piece::WKING: case Piece::BKING:
        case Piece::WPAWN: case Piece::BPAWN:
        case Piece::EMPTY:
            break;
        default:
            tmpPos.setPiece(sq, Piece::EMPTY);
        }
    }

    for (int i = idx; i < (int)kernel.nodes.size(); i++) {
        const ExtPkMove& m = kernel.nodes[i].move;
        int p = Piece::EMPTY;
        if (m.fromSquare != -1) {
            p = tmpPos.getPiece(m.fromSquare);
            tmpPos.setPiece(m.fromSquare, Piece::EMPTY);
        }
        if (m.promotedPiece != PieceType::EMPTY)
            p = Piece::EMPTY;
        tmpPos.setPiece(m.toSquare, p);
    }

    auto countPawns = [](const Position& pos, int sq, bool white) -> int {
        U64 mask = 1ULL << sq;
        mask = white ? BitBoard::southFill(mask) : BitBoard::northFill(mask);
        mask &= pos.pieceTypeBB(white ? Piece::WPAWN : Piece::BPAWN);
        return BitBoard::bitCount(mask);
    };

    auto pawnsOk = [this,&tmpPos,&countPawns](bool white, int x) -> bool {
        U64 mask = goalPos.pieceTypeBB(white ? Piece::WPAWN : Piece::BPAWN);
        mask &= BitBoard::maskFile[x];
        while (mask != 0) {
            int sq = BitBoard::extractSquare(mask);
            int cntNow = countPawns(tmpPos, sq, white);
            int cntGoal = countPawns(goalPos, sq, white);
            if (cntNow < cntGoal)
                return false;
        }
        return true;
    };

    for (int c = 0; c < 2; c++) {
        bool white = c == 0;
        U64 mask = tmpPos.pieceTypeBB(white ? Piece::WPAWN : Piece::BPAWN);
        while (mask != 0) {
            int sq = BitBoard::extractSquare(mask);
            int x0 = Square::getX(sq);
            int y0 = Square::getY(sq);
            for (int d = 1; d <= 2; d++) {
                if (d == 2 && y0 != (white ? 1 : 6))
                    break;
                int y1 = y0 + (white ? d : -d);
                if (y1 == 0 || y1 == 7)
                    break; // Cannot move pawn to first/last rank
                int toSq = Square::getSquare(x0, y1);
                if (tmpPos.getPiece(toSq) != Piece::EMPTY)
                    break;

                Move m(sq, toSq, Piece::EMPTY);
                UndoInfo ui;
                tmpPos.makeMove(m, ui);

                if (pawnsOk(white, x0)) {
                    ExtPkMove em(white ? PieceColor::WHITE : PieceColor::BLACK,
                                 PieceType::PAWN, sq, false, toSq, PieceType::EMPTY);
                    pawnMoves.push_back(em);
                }

                tmpPos.unMakeMove(m, ui);
            }
        }
    }
}

// --------------------------------------------------------------------------------

void
PkSequence::Graph::addNode(const ExtPkMove& m) {
    nodes.emplace_back(nextId++, m);

    MoveData& md = nodes.back();
    if (m.movingPiece == PieceType::PAWN) {
        md.pseudoLegal = true;
        if (m.capture && nodes.size() > 1) {
            const MoveData& prev = nodes[nodes.size() - 2];
            if (m.toSquare == prev.move.toSquare)
                md.dependsOn.push_back(prev.id);
        }
        U64 mMask = (1ULL << m.fromSquare) | (1ULL << m.toSquare);
        for (int i = (int)nodes.size() - 2; i >= 0; i--) {
            const MoveData& prev = nodes[i];
            if (prev.move.movingPiece == PieceType::PAWN) {
                U64 iMask = (1ULL << prev.move.fromSquare) | (1ULL << prev.move.toSquare);
                if ((mMask & iMask) != 0)
                    md.dependsOn.push_back(prev.id);
            }
        }
    }
}

void
PkSequence::Graph::replaceNode(int idx, const std::vector<ExtPkMove>& moves) {
    if (moves.empty()) {
        nodes.erase(nodes.begin() + idx);
        return;
    }

    const int oldId = nodes[idx].id;
    auto dependsOn = nodes[idx].dependsOn;
    nodes[idx] = MoveData(nextId++, moves[0]);
    nodes[idx].pseudoLegal = true;
    nodes[idx].dependsOn = dependsOn;

    std::vector<MoveData> toInsert;
    int prevId = nodes[idx].id;
    for (int i = 1; i < (int)moves.size(); i++) {
        MoveData md(nextId++, moves[i]);
        md.pseudoLegal = true;
        md.dependsOn.push_back(prevId);
        toInsert.push_back(md);
        prevId = md.id;
    }
    nodes.insert(nodes.begin() + (idx + 1), toInsert.begin(), toInsert.end());

    for (MoveData& md : nodes)
        for (int& d : md.dependsOn)
            if (d == oldId)
                d = prevId;
}

bool
PkSequence::Graph::topoSort() {
    const int n = nodes.size();
    std::vector<bool> visited(n, false);
    std::vector<bool> currPath(n, false);

    std::vector<int> idToIdx(nextId, -1);
    for (int i = 0; i < n; i++)
        idToIdx[nodes[i].id] = i;

    std::vector<MoveData> result;
    for (int i = 0; i < n; i++)
        if (!sortRecursive(i, visited, currPath, idToIdx, result))
            return false;

    nodes = result;
    return true;
}

bool
PkSequence::Graph::sortRecursive(int i,
                                 std::vector<bool>& visited,
                                 std::vector<bool>& currPath,
                                 const std::vector<int>& idToIdx,
                                 std::vector<MoveData>& result) {
    if (currPath[i])
        return false; // Cyclic

    if (visited[i])
        return true;
    visited[i] = true;

    currPath[i] = true;
    for (int dep : nodes[i].dependsOn)
        if (!sortRecursive(idToIdx[dep], visited, currPath, idToIdx, result))
            return false;
    currPath[i] = false;

    result.push_back(nodes[i]);
    return true;
}
