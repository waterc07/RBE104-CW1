#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game.h"
#include "scene.h"

static void testSharedCommands(void)
{
    Position expectedRobot, expectedForklifts[MAX_FORKLIFTS];
    char expectedMap[ROWS][COLS + 1];
    int i, expectedRandom;
    const char *route = "sssdawww";

    initialise(1, 42);
    for (i = 0; route[i]; i++) {
        processCommand(route[i]);
    }
    expectedRobot = robot;
    memcpy(expectedForklifts, forklifts, sizeof expectedForklifts);
    memcpy(expectedMap, map, sizeof expectedMap);
    expectedRandom = rand();

    /* Pause and rejected movement must not change the random sequence. */
    processCommand('r');
    assert(!paused && turns == 0 && score == 0 && lives == 3);
    assert(forkCount == 1 && totalPackages == 3 && !carrying && !delivered);
    assert(map[4][2] == 'P');
    processCommand('p');
    assert(paused);
    assert(processCommand('s') == 0 && takeTurn('d') == 0 && turns == 0);
    processCommand('p');
    assert(!paused);
    for (i = 0; route[i]; i++) {
        processCommand(route[i]);
    }
    assert(turns == 8 && score == 100 && delivered == 1 && lives == 3 && !carrying);
    assert(robot.row == expectedRobot.row && robot.col == expectedRobot.col);
    assert(memcmp(forklifts, expectedForklifts, sizeof expectedForklifts) == 0);
    assert(memcmp(map, expectedMap, sizeof expectedMap) == 0);
    assert(rand() == expectedRandom);

    processCommand('p');
    processCommand('r');
    assert(!paused && turns == 0 && getGameStatus() == GAME_PLAYING);
    /* Set up final-delivery outcomes directly; these are not full playthroughs. */
    robot.row = 1;
    robot.col = 2;
    carrying = 1;
    delivered = totalPackages - 1;
    processCommand('a');
    assert(getGameStatus() == GAME_WON && score == 100);
    assert(processCommand('s') == 0 && turns == 1);
    processCommand('p');
    assert(!paused);
    processCommand('r');
    assert(getGameStatus() == GAME_PLAYING && !delivered && score == 0);

    /* Force the forklift to enter D after the final delivery. */
    robot.row = 1;
    robot.col = 2;
    carrying = 1;
    delivered = totalPackages - 1;
    lives = 1;
    forklifts[0].row = 2;
    forklifts[0].col = 1;
    map[3][1] = '#';
    processCommand('a');
    assert(delivered == totalPackages && lives == 0 && getGameStatus() == GAME_LOST);
    assert(processCommand('s') == 0 && turns == 1);
    processCommand('p');
    assert(!paused);
    processCommand('r');
    assert(getGameStatus() == GAME_PLAYING && lives == 3 && turns == 0);
    assert(map[3][1] == ' ');
    puts("PASS: shared pause, deterministic restart, win/loss gating and fatal-delivery priority.");
}

int main(void)
{
    int i;
    const char *route="sssdawww";
    initialise(1,42);
    for(i=0;route[i];i++) takeTurn(route[i]);
    assert(score==100 && delivered==1 && lives==3 && !carrying);
    drawScene(42,0);
    assert(savePreview("delivered.ppm"));
    puts("PASS: seed 42, one forklift, sssdawww: delivery, score, lives and HUD draw.");
    initialise(1,42);
    assert(takeTurn('w')==0 && turns==0);
    assert(takeTurn('x')==0 && turns==0);
    puts("PASS: invalid input and blocked movement do not use a turn.");
    for(i=0;i<3;i++) {
        forklifts[0].row=1;forklifts[0].col=2;
        takeTurn('d');
        assert(lives==2-i && robot.row==1 && robot.col==1);
    }
    drawScene(42,0);
    puts("PASS: collisions cost one life, respawn, three collisions cause loss.");
    initialise(1,42);
    forklifts[0].row=1;forklifts[0].col=3;map[1][4]='#';
    takeTurn('d');
    assert(lives==2 && robot.col==1);
    puts("PASS: forklift moving into robot detected.");
    initialise(1,42);
    carrying=1;robot.row=4;robot.col=1;
    takeTurn('d');
    assert(carrying==1 && map[4][2]=='P');
    drawScene(42,0);
    puts("PASS: one-package limit and carrying HUD draw.");
    testSharedCommands();
    puts("SDL keyboard / resize behaviour still requires an actual window check.");
    return 0;
}
