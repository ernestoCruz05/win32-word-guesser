#pragma once
#pragma once

#ifndef TP_SO2_H
#define TP_SO2_H

#include <stdio.h>
#include <windows.h>
#include <tchar.h>
#include <io.h>
#include <fcntl.h>
#include <process.h>
#include <conio.h> 


#define MAX_WORD_LENGTH 12
#define BUFFER_SIZE 10
#define MAX_NAME_LENGTH 25
#define MAX_PLAYERS 10
#define MAX_MAXLETTERS 12

#define SHM_NAME _T("SHM_LETTERS")
#define LETTER_MUTEX_NAME _T("LETTERS_MUTEX")
#define COMMAND_MUTEX_NAME _T("COMMAND_MUTEX")
#define SEM_EMPTY_POS_LETTER _T("SEM_LETTER")
#define SEM_PLAYER_CONT _T("PLAYER_CONTROL")

#define PIPE_NAME _T("\\\\.\\pipe\\SO2")
#define PIPE_BUFFER_SIZE 512

#define MSG_REGISTER	1
#define MSG_GUESS		2
#define MSG_COMMAND		3
#define MSG_RESPONSE	4
#define MSG_BROADCAST	5
#define MSG_DISCONNECT	6
#define MSG_KICK		7


typedef struct
{
	HANDLE hPipe;
	TCHAR name[20];
	int points;
	int active;
} PLAYER;

typedef struct
{
	int msgType;
	TCHAR sender[ MAX_NAME_LENGTH ];
	TCHAR content[256];
}GAME_MESSAGE;


typedef struct
{
	char letters[12];				//abcR
	char displayedLetters[12];		//abcDisplay
	int letterInfo[12][2];			//abcInfo
	int playerCount;
	PLAYER playerList[MAX_PLAYERS];
	int running;					// 1-> The game is running (i.e 1 or more players are connected), 0-> game is stopped
} GameSharedMem;

typedef struct
{
	char letters[12];
	int lettercount;
	HANDLE hMutex;
} SHM_LETTERS;

typedef struct
{
	int shouldContinue;			// 1-> continue, 0 -> stop
	HANDLE hMapFile;
	GameSharedMem* sharedMem;
	HANDLE hGameMutex;
	HANDLE hLetterSemaphore;
	HANDLE hPlayerSemaphore;
	HANDLE hCommandMutex;
} GameControlData;



#endif 