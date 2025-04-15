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

#define MAXLETRAS 10
#define RITMO 5000
#define MAX_WORD_LENGTH 20
#define BUFFER_SIZE 10
#define MAX_NAME_LENGTH 25
#define MAX_PLAYERS 10


#define SHM_NAME TEXT("SHM_LETTERS")
#define LETTER_MUTEX_NAME TEXT("LETTERS_MUTEX")
#define COMMAND_MUTEX_NAME TEXT("COMMAND_MUTEX")
#define SEM_EMPTY_POS_LETTER TEXT("SEM_LETTER")
#define SEM_PLAYER_CONT TEXT("PLAYER_CONTROL")

typedef struct
{
	TCHAR name[20];
	int points;
	int active;
} PLAYER;

typedef struct
{
	char letters[MAXLETRAS];				//abcR
	char displayedLetters[MAXLETRAS];		//abcDisplay
	int letterInfo[MAXLETRAS][2];			//abcInfo
	int playerCount;
	PLAYER playerList[MAX_PLAYERS];
	unsigned int running;					// 1-> The game is running (i.e 1 or more players are connected), 0-> game is stopped
} GameSharedMem;

typedef struct
{
	char letters[MAXLETRAS];
	int lettercount;
	HANDLE hMutex;
} SHM_LETTERS;

typedef struct
{
	unsigned int shouldContinue;			// 1-> continue, 0 -> stop
	HANDLE hMapFile;
	GameSharedMem* sharedMem;
	HANDLE hGameMutex;
	HANDLE hLetterSemaphore;
	HANDLE hPlayerSemaphore;
	HANDLE hCommandMutex;
} GameControlData;



#endif 