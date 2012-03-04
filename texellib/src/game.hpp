/*
 * game.hpp
 *
 *  Created on: Feb 25, 2012
 *      Author: petero
 */

#ifndef GAME_HPP_
#define GAME_HPP_

#include "move.hpp"
#include "undoInfo.hpp"
#include "position.hpp"
#include "player.hpp"

#include <vector>
#include <string>

/**
 * Handles a game between two players.
 */
class Game {
public:
    enum GameState {
        ALIVE,
        WHITE_MATE,         // White mates
        BLACK_MATE,         // Black mates
        WHITE_STALEMATE,    // White is stalemated
        BLACK_STALEMATE,    // Black is stalemated
        DRAW_REP,           // Draw by 3-fold repetition
        DRAW_50,            // Draw by 50 move rule
        DRAW_NO_MATE,       // Draw by impossibility of check mate
        DRAW_AGREE,         // Draw by agreement
        RESIGN_WHITE,       // White resigns
        RESIGN_BLACK        // Black resigns
    };

    Position pos;
    Player* whitePlayer;
    Player* blackPlayer;

protected:
    std::vector<Move> moveList;
    std::vector<UndoInfo> uiInfoList;
    std::vector<bool> drawOfferList;
    int currentMove;

private:
    std::string drawStateMoveStr; // Move required to claim DRAW_REP or DRAW_50
    GameState resignState;

public:
    bool pendingDrawOffer;
    GameState drawState;

    Game(Player& whitePlayer, Player& blackPlayer) {
        this->whitePlayer = &whitePlayer;
        this->blackPlayer = &blackPlayer;
        handleCommand("new");
    }

    /**
     * Update the game state according to move/command string from a player.
     * @param str The move or command to process.
     * @return True if str was understood, false otherwise.
     */
    bool processString(const std::string& str);

    std::string getGameStateString();

    /**
     * Get the last played move, or null if no moves played yet.
     */
    Move getLastMove();

    /**
     * Get the current state of the game.
     */
    GameState getGameState();

    /**
     * Check if a draw offer is available.
     * @return True if the current player has the option to accept a draw offer.
     */
    bool haveDrawOffer();

    void getPosHistory(std::vector<std::string> ret);

    std::string getMoveListString(bool compressed);

    std::string getPGNResultString();

    /** Return a list of previous positions in this game, back to the last "zeroing" move. */
    void getHistory(std::vector<Position>& posList);

    static U64 perfT(Position& pos, int depth);

protected:
    /**
     * Handle a special command.
     * @param moveStr  The command to handle
     * @return  True if command handled, false otherwise.
     */
    bool handleCommand(const std::string& moveStr);

    /** Swap players around if needed to make the human player in control of the next move. */
    void activateHumanPlayer();

private:
    /**
     * Print a list of all moves.
     */
    void listMoves();

    bool handleDrawCmd(std::string drawCmd);

    bool handleBookCmd(const std::string& bookCmd);

    bool insufficientMaterial();

    // Not implemented
    Game(const Game& other);
    Game& operator=(const Game& other);
};


#endif /* GAME_HPP_ */