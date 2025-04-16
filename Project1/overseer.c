/*
 *Funções:
 1 -> Sortear as letras <feito>
 2 -> Reconhecar as palavras através de um dicionario <metade feito, falta integreção de pipes>
 3 -> Gerir jogadores <implementado apenas a lida, falta a logica>
 4 -> Pontuações <implementado apenas a lida, falta a logica>
 5 -> Lançar um bot <implementado apenas a lida, falta a logica>
 6 -> Ser administrador <implementado apenas a lida, falta a logica>
 *
 *
 * Restrições:
 1 -> Apenas uma instancia unica e precisa de estar aberto para o jogo andar, tipo o manager do trabalho de SO
 2 -> Precisa de poder utilizar o comando para lançar comandos sem parar os restos das funções (threads)
 *
 *
 * Anunciar:
 1 -> Jogadores que entram no jogo
 2 -> Jogadores que abandonam o jogo
 3 -> Jogadores que adivinham a palavra
 4 -> Avisar quando um jogador passa para o topo, ou seja, ultrapassa os outros em pontuação
 */

 /*
  * Sortear as letras
  - Ter um array abecedario, fazer um rand() MAXLETRAS vezes com um range de [0 , 25]
  - Enviar a informação ao jogoui
  */


/*
 * Logica do cliente se desconectar
 * Anunciar o "lider" da pontuação 
 *
 *
 */

#include "utils.h"

int MAXLETRAS = 6;
int RITMO = 3000;

HANDLE hMutex;
HANDLE hSemaphore;
char abc[] = "abcdefghijklmnopqrstuvwxyz";
char abcR[MAX_MAXLETTERS] = { '_', '_', '_', '_', '_', '_', '_', '_', '_', '_' };
char abcRDisplay[MAX_MAXLETTERS] = { '_', '_', '_', '_', '_', '_', '_', '_', '_', '_' };
int abcRInfo[MAX_MAXLETTERS][2]; // [Letra, Posição no Display]

TCHAR wordBuffer[BUFFER_SIZE][MAX_WORD_LENGTH];
int bufferStart = 0;
int bufferEnd = 0;

int broadFlag = 0;

TCHAR dictionary[52][10] = {
	_T("casa"), _T("pato"), _T("mesa"), _T("fogo"), _T("nuvem"),
	_T("vento"), _T("pedra"), _T("chao"), _T("livro"), _T("porta"),
	_T("faca"), _T("rio"), _T("chave"), _T("leite"), _T("canto"),
	_T("velha"), _T("tres"), _T("verde"), _T("folha"), _T("lápis"),
	_T("papel"), _T("areia"), _T("brisa"), _T("barco"), _T("dente"),
	_T("campo"), _T("luz"), _T("corpo"), _T("ninho"), _T("tempo"),
	_T("peixe"), _T("pomba"), _T("manga"), _T("vento"), _T("cobra"),
	_T("poder"), _T("sabor"), _T("gosto"), _T("troca"), _T("morte"),
	_T("ferro"), _T("marca"), _T("grito"), _T("falta"), _T("prato"),
	_T("cinto"), _T("sorte"), _T("tigre"), _T("vazio"), _T("magia"),
	_T("hhp"), _T("lnl")
};
PLAYER playerList[10];
int playerCount = 0;


BOOL LoadOrCreateRegistrySettings() {
	HKEY hKey;
	DWORD disp;

	LONG result = RegCreateKeyEx(
		HKEY_CURRENT_USER,
		_T("Software\\TrabSO2"),
		0,
		NULL,
		0,
		KEY_ALL_ACCESS,
		NULL,
		&hKey,
		&disp
	);

	if (result != ERROR_SUCCESS) {
		_tprintf(_T("Error either opening or creating the key-values\n"));
		return FALSE;
	}

	DWORD size = sizeof(DWORD);
	DWORD defaultMAX = 10;
	DWORD defaultRITMO = 15000;

	DWORD tmp;

	if (RegQueryValueEx(hKey, _T("MAXLETRAS"), NULL, NULL, (LPBYTE)&tmp, &size) != ERROR_SUCCESS) {
		RegSetValueEx(hKey, _T("MAXLETRAS"), 0, REG_DWORD, (const BYTE*)&defaultMAX, sizeof(DWORD));
		MAXLETRAS = defaultMAX;
	}
	else {
		MAXLETRAS = tmp;
	}

	if (RegQueryValueEx(hKey, _T("RITMO"), NULL, NULL, (LPBYTE)&tmp, &size) != ERROR_SUCCESS) {
		RegSetValueEx(hKey, _T("RITMO"), 0, REG_DWORD, (const BYTE*)&defaultRITMO, sizeof(DWORD));
		RITMO = defaultRITMO;
	}
	else {
		RITMO = tmp;
	}

	RegCloseKey(hKey);

	_tprintf(_T("MAXLETRAS = %d | RITMO = %d ms\n"), MAXLETRAS, RITMO);
	return TRUE;
}

BOOL initResources(GameControlData* cdata)
{
	cdata->hMapFile = CreateFileMapping(
		INVALID_HANDLE_VALUE,
		NULL,
		PAGE_READWRITE,
		0,
		sizeof(GameSharedMem),
		SHM_NAME
	);

	if (cdata->hMapFile == NULL)
	{
		return FALSE;
	}

	cdata->sharedMem = (GameSharedMem*)MapViewOfFile(
		cdata->hMapFile,
		FILE_MAP_ALL_ACCESS,
		0,
		0,
		sizeof(GameSharedMem)
	);

	if (cdata->sharedMem == NULL)
	{
		CloseHandle(cdata->hMapFile);
		return FALSE;
	}

	cdata->hGameMutex = CreateMutex(
		NULL,
		FALSE,
		LETTER_MUTEX_NAME)
		;

	cdata->hCommandMutex = CreateMutex(
		NULL,
		FALSE,
		COMMAND_MUTEX_NAME
	);

	cdata->hPlayerSemaphore = CreateSemaphore(
		NULL,
		0,
		MAX_PLAYERS,
		SEM_PLAYER_CONT
	);

	cdata->hLetterSemaphore = CreateSemaphore(
		NULL,
		1,
		MAXLETRAS,
		SEM_EMPTY_POS_LETTER
	);

	WaitForSingleObject(cdata->hGameMutex, INFINITE);
	memset(cdata->sharedMem->letters, '_', MAXLETRAS);
	memset(cdata->sharedMem->displayedLetters, '_', MAXLETRAS);
	memset(cdata->sharedMem->letterInfo, 0, sizeof(int) * MAXLETRAS * 2);
	cdata->sharedMem->playerCount = 0;
	cdata->sharedMem->running = 0;
	ReleaseMutex(cdata->hGameMutex);

	cdata->shouldContinue = 1;

	return TRUE;
}




void printABCR() {
	_tprintf(_T("\n\nabcR (Oldest comes first): "));
	for (int i = 0; i < MAXLETRAS; i++) {
		_tprintf(_T("%c "), abcR[i]);
	}
	_tprintf(_T("\n"));
}

void printABCRDisplay() {
	_tprintf(_T("\n\nabcRDisplay (What players see): "));
	for (int i = 0; i < MAXLETRAS; i++) {
		_tprintf(_T("%c "), abcRDisplay[i]);
	}
	_tprintf(_T("\n"));
}

void createLetter(int pos) {
	int r = rand() % 26;
	abcR[pos] = abc[r];

	for (int i = 0; i < MAXLETRAS; i++) {
		if (abcRDisplay[i] == '_') {
			abcRDisplay[i] = abc[r];
			abcRInfo[pos][0] = abc[r];
			abcRInfo[pos][1] = i;
			break;
		}
	}
}

void removeOldest() {
	int posRemoved = abcRInfo[0][1];

	for (int i = 0; i < MAXLETRAS - 1; i++) {
		abcR[i] = abcR[i + 1];
		abcRInfo[i][0] = abcRInfo[i + 1][0];
		abcRInfo[i][1] = abcRInfo[i + 1][1];
	}

	abcR[MAXLETRAS - 1] = '_';
	abcRInfo[MAXLETRAS - 1][0] = '_';
	abcRInfo[MAXLETRAS - 1][1] = -1;

	abcRDisplay[posRemoved] = '_';
}

void copyData(GameControlData* cdata)
{
	memcpy(cdata->sharedMem->displayedLetters, abcRDisplay, sizeof(abcRDisplay));
	memcpy(cdata->sharedMem->letters, abcR, sizeof(abcR));
	memcpy(cdata->sharedMem->letterInfo, abcRInfo, sizeof(abcRInfo));
}

DWORD WINAPI GeraLetrasThread(LPVOID pArguments) {
	GameControlData* cdata = (GameControlData*)pArguments;
	int cont = 0;

	while (1) {
		Sleep(RITMO);


		for (int i = 0; i < MAXLETRAS; i++) {
			if (abcR[i] == '_') {
				createLetter(i);
				copyData(cdata);
				cont = 0;
			}
			else {
				cont++;
			}
		}

		if (cont == MAXLETRAS) {
			removeOldest();
			createLetter(MAXLETRAS - 1);
			copyData(cdata);
		}

		//printABCRDisplay();
		//escreveABCR();
		cont = 0;
		ReleaseSemaphore(cdata->hLetterSemaphore, 1, NULL);
		ReleaseMutex(cdata->hGameMutex);
	}
	return 0;
}

void countLetters(char* arr, int* count) {
	for (int i = 0; i < 26; i++) {
		count[i] = 0;
	}

	for (int i = 0; i < MAXLETRAS; i++) {
		if (arr[i] >= 'a' && arr[i] <= 'z') {
			count[arr[i] - 'a']++;
		}
	}
	_tprintf(_T("Available letters:\n"));
	for (int i = 0; i < 26; i++) {
		if (count[i] > 0) {
			_tprintf(_T("%c: %d\n"), 'a' + i, count[i]);
		}
	}
	_tprintf(_T("\n"));
}

int validateGuess(TCHAR* guess) {
	int needed[26] = { 0 };
	int have[26] = { 0 };
	int found = 0;

	_tprintf(_T("Validating guess: %s\n"), guess);

	_tprintf(_T("Number of letters available:\n"));
	countLetters(abcR, needed);

	_tprintf(_T("Number of letters on the guess:\n"));
	for (int i = 0; guess[i] != '\0'; i++) {
		if (guess[i] >= 'a' && guess[i] <= 'z') {
			have[guess[i] - 'a']++;
		}
	}
	for (int i = 0; i < 26; i++) {
		if (have[i] > 0) {
			_tprintf(_T("%c: %d\n"), 'a' + i, have[i]);
		}
	}

	_tprintf(_T("Validation:\n"));
	for (int i = 0; i < 26; i++) {
		if (have[i] > 0) {
			_tprintf(_T("%c: Have=%d, Need=%d - %s\n"),
				'a' + i,
				have[i],
				needed[i],
				(have[i] <= needed[i]) ? _T("OK") : _T("Missing letters"));
		}
	}

	for (int i = 0; i < 26; i++) {
		if (have[i] > needed[i]) {
			_tprintf(_T("Word is not valid, letters are not available\n"));
			return 0;
		}
	}

	for (int i = 0; i < 51; i++) {
		if (_tcscmp(guess, dictionary[i]) == 0) {
			found = 1;
			break;
		}
	}

	if (!found) {
		_tprintf(_T("Palavra não encontrada no dicionário\n"));
		return 0;
	}

	WaitForSingleObject(hMutex, INFINITE);

	for (int i = 0; guess[i] != '\0'; i++) {
		if (guess[i] >= 'a' && guess[i] <= 'z') {
			for (int j = 0; j < MAXLETRAS; j++) {
				if (abcR[j] == guess[i]) {
					int posRemoved = abcRInfo[j][1];

					for (int k = j; k < MAXLETRAS - 1; k++) {
						abcR[k] = abcR[k + 1];
						abcRInfo[k][0] = abcRInfo[k + 1][0];
						abcRInfo[k][1] = abcRInfo[k + 1][1];
					}

					abcR[MAXLETRAS - 1] = '_';
					abcRInfo[MAXLETRAS - 1][0] = '_';
					abcRInfo[MAXLETRAS - 1][1] = -1;

					if (posRemoved != -1) {
						abcRDisplay[posRemoved] = '_';
					}

					break;
				}
			}
		}
	}

	ReleaseMutex(hMutex);

	_tprintf(_T("Valid guess!\n"));
	return 1;
}

DWORD WINAPI GuessValidationThread(void* pArguments) {
	while (1) {
		WaitForSingleObject(hSemaphore, INFINITE);

		WaitForSingleObject(hMutex, INFINITE);

		TCHAR currentWord[MAX_WORD_LENGTH];
		_tcscpy_s(currentWord, MAX_WORD_LENGTH, wordBuffer[bufferStart]);
		bufferStart = (bufferStart + 1) % BUFFER_SIZE;

		ReleaseMutex(hMutex);

		validateGuess(currentWord);
	}
	return 0;
}

void addWordToBuffer(TCHAR* word) {
	WaitForSingleObject(hMutex, INFINITE);

	_tcscpy_s(wordBuffer[bufferEnd], MAX_WORD_LENGTH, word);
	bufferEnd = (bufferEnd + 1) % BUFFER_SIZE;

	ReleaseMutex(hMutex);

	ReleaseSemaphore(hSemaphore, 1, NULL);
}

void showGeneralHelp() {
	_tprintf(_T("\nAvailable commands:\n"));
	_tprintf(_T("? - Show this help\n"));
	_tprintf(_T("kick <player> - Remove a player\n"));
	_tprintf(_T("show ? - Show display options\n"));
	_tprintf(_T("bot ? - Show bot commands\n"));
	_tprintf(_T("game ? - Show game commands\n"));
	_tprintf(_T("exit - Quit the overseer\n"));
}

void showDisplayHelp() {
	_tprintf(_T("\nDisplay options:\n"));
	_tprintf(_T("show leaderboard - Show scores\n"));
	_tprintf(_T("show playerlist - List all players\n"));
	_tprintf(_T("show mem - Purely a command to debug the information on the shared memory"));
}

void showBotHelp() {
	_tprintf(_T("\nBot commands:\n"));
	_tprintf(_T("bot create - Add a bot player\n"));
	_tprintf(_T("bot remove <name> - Remove a bot\n"));
}

void showGameHelp() {
	_tprintf(_T("\nGame commands:\n"));
	_tprintf(_T("game end - ends game (if there is a game runnig)\n"));
	_tprintf(_T("game start - (if there isnt a game runnig)\n"));
}



void displaySharedMemory(GameControlData* cdata) {
	WaitForSingleObject(cdata->hGameMutex, INFINITE);

	_tprintf(_T("\n=== Shared Memory Contents ===\n"));
	_tprintf(_T("Displayed Letters: "));
	for (int i = 0; i < MAXLETRAS; i++) {
		_tprintf(_T("%c "), cdata->sharedMem->displayedLetters[i]);
	}

	_tprintf(_T("\nInternal Letters:  "));
	for (int i = 0; i < MAXLETRAS; i++) {
		_tprintf(_T("%c "), cdata->sharedMem->letters[i]);
	}

	_tprintf(_T("\nLetter Info:\n"));
	for (int i = 0; i < MAXLETRAS; i++) {
		_tprintf(_T("Pos %d: Char=%c, DisplayPos=%d\n"),
			i,
			cdata->sharedMem->letterInfo[i][0],
			cdata->sharedMem->letterInfo[i][1]);
	}

	_tprintf(_T("Players: %d\n"), cdata->sharedMem->playerCount);
	_tprintf(_T("Game Status: %s\n"), cdata->sharedMem->running ? _T("Running") : _T("Stopped"));

	ReleaseMutex(cdata->hGameMutex);
}

void broadcastMaker(GameControlData* cdata, const TCHAR* messageContent)
{
	GAME_MESSAGE broadcast;
	broadcast.msgType = MSG_BROADCAST;
	_tcscpy_s(broadcast.sender, MAX_NAME_LENGTH, _T("SERVER"));
	_tcscpy_s(broadcast.content, 256, messageContent);

	for (int i = 0; i < cdata->sharedMem->playerCount; i++) {
		if (cdata->sharedMem->playerList[i].active && cdata->sharedMem->playerList[i].hPipe != NULL) {
			DWORD written;
			WriteFile(cdata->sharedMem->playerList[i].hPipe, &broadcast, sizeof(GAME_MESSAGE), &written, NULL);
		}
	}
}

void processCommand(TCHAR* command, GameControlData* cdata) {
	TCHAR cmd1[20], cmd2[20];
	TCHAR param[20];

	if (_stscanf_s(command, _T("%19s %19s %19s"),
		cmd1, (unsigned)_countof(cmd1),
		cmd2, (unsigned)_countof(cmd2),
		param, (unsigned)_countof(param)) >= 1) {

		WaitForSingleObject(hMutex, INFINITE);

		if (_tcscmp(cmd1, _T("?")) == 0) {
			showGeneralHelp();
		}
		else if (_tcscmp(cmd1, _T("game")) == 0 && _tcscmp(cmd2, _T("?")) == 0) {
			showGameHelp();
		}
		else if (_tcscmp(cmd1, _T("show")) == 0 && _tcscmp(cmd2, _T("?")) == 0) {
			showDisplayHelp();
		}
		else if (_tcscmp(cmd1, _T("bot")) == 0 && _tcscmp(cmd2, _T("?")) == 0) {
			showBotHelp();
		}
		else if (_tcscmp(cmd1, _T("kick")) == 0) {
			if (_tcslen(cmd2) == 0) {
				_tprintf(_T("Usage: kick <playername>\n"));
				ReleaseMutex(hMutex);
				return;
			}

			BOOL found = FALSE;
			for (int i = 0; i < cdata->sharedMem->playerCount; i++) {
				if (cdata->sharedMem->playerList[i].active &&
					_tcscmp(cdata->sharedMem->playerList[i].name, cmd2) == 0) {

					if (cdata->sharedMem->playerList[i].hPipe != NULL) {
						GAME_MESSAGE kickMsg;
						kickMsg.msgType = MSG_KICK;
						_tcscpy_s(kickMsg.sender, MAX_NAME_LENGTH, _T("SERVER"));
						_tcscpy_s(kickMsg.content, 256, _T("You have been kicked by the overseer. The game will now close."));

						DWORD written;
						WriteFile(cdata->sharedMem->playerList[i].hPipe, &kickMsg, sizeof(GAME_MESSAGE), &written, NULL);

						Sleep(200);

						CloseHandle(cdata->sharedMem->playerList[i].hPipe);
						cdata->sharedMem->playerList[i].hPipe = NULL;
					}

					int last = cdata->sharedMem->playerCount - 1;
					if (i != last) {
						cdata->sharedMem->playerList[i] = cdata->sharedMem->playerList[last];
					}

					memset(&cdata->sharedMem->playerList[last], 0, sizeof(PLAYER));
					cdata->sharedMem->playerCount--;

					_tprintf(_T("Player %s has been kicked from the game.\n"), cmd2);

					TCHAR msg[256];
					_stprintf_s(msg, 256, _T("Player %s was kicked from the game by the overseer."), cmd2);
					broadcastMaker(cdata, msg);

					found = TRUE;
					break;
				}
			}

			if (!found) {
				_tprintf(_T("No active player named '%s' found.\n"), cmd2);
			}
		}
		else if (_tcscmp(cmd1, _T("show")) == 0) {
			if (_tcscmp(cmd2, _T("leaderboard")) == 0) {
				_tprintf(_T("\n=== Leaderboard ===\n"));
				for (int i = 0; i < MAX_PLAYERS; i++) {
					if (cdata->sharedMem->playerList[i].active) {
						_tprintf(_T("- %s: %d pontos\n"),
							cdata->sharedMem->playerList[i].name,
							cdata->sharedMem->playerList[i].points);
					}
				}
			}
			else if (_tcscmp(cmd2, _T("playerlist")) == 0) {
				_tprintf(_T("\n=== Player List ===\n"));
				for (int i = 0; i < MAX_PLAYERS; i++) {
					if (cdata->sharedMem->playerList[i].active) {
						_tprintf(_T("- %s\n"), cdata->sharedMem->playerList[i].name);
					}
				}
			}
		}
		else if (_tcscmp(cmd1, _T("game")) == 0) {
			if (_tcscmp(cmd2, _T("end")) == 0) {
				if (cdata->sharedMem->running == 1) {
					_tprintf(_T("\nClosing game...\n"));
					_tprintf(_T("\n=== Final Leaderboard ===\n"));
					for (int i = 0; i < MAX_PLAYERS; i++) {
						if (cdata->sharedMem->playerList[i].active) {
							_tprintf(_T("- %s: %d pontos\n"),
								cdata->sharedMem->playerList[i].name,
								cdata->sharedMem->playerList[i].points);
						}
					}
					_tprintf(_T("\nPlayer %s won!!\n"), cdata->sharedMem->playerList[0].name);
					cdata->sharedMem->running = 0;
				}
				else {
					_tprintf(_T("\nNo game is currently running.\n"));
				}
			}
			else if (_tcscmp(cmd2, _T("start")) == 0) {
				if (cdata->sharedMem->running == 0) {
					if (cdata->sharedMem->playerCount < 2) {
						_tprintf(_T("\nNot enough players to start the game.\n"));
					}else{
					_tprintf(_T("\nStarting game...\n"));
					cdata->sharedMem->running = 1;
					broadcastMaker(cdata, _T("\nGame is starting!\n")); 
					}
				}
				else {
					_tprintf(_T("\nGame is already running.\n"));
				}
			}
		}
		else if (_tcscmp(cmd1, _T("bot")) == 0) {
			if (_tcscmp(cmd2, _T("create")) == 0) {
				_tprintf(_T("comando bot create"));
			}
			else if (_tcscmp(cmd2, _T("remove")) == 0) {
				_tprintf(_T("comando bot remove"));
			}
		} else if (_tcscmp(cmd1, _T("rythm")) == 0)
		{
			if (_tcscmp(cmd2, _T("up")) == 0)
			{
				if (RITMO > 1000) {
					RITMO -= 1000;
				}
				else {
					_tprintf(_T("RYTHM is already at its lowest point(1000 ms).\n"));
					ReleaseMutex(hMutex);
					return;
				}

				HKEY hKey;
				if (RegOpenKeyEx(HKEY_CURRENT_USER, _T("Software\\TrabSO2"), 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
					RegSetValueEx(hKey, _T("RITMO"), 0, REG_DWORD, (const BYTE*)&RITMO, sizeof(DWORD));
					RegCloseKey(hKey);
				}

				_tprintf(_T("New speed (RYTHM): %d ms\n"), RITMO);
			} else if (_tcscmp(cmd2, _T("down")) == 0)
			{
				RITMO += 1000;

				HKEY hKey;
				if (RegOpenKeyEx(HKEY_CURRENT_USER, _T("Software\\TrabSO2"), 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
					RegSetValueEx(hKey, _T("RITMO"), 0, REG_DWORD, (const BYTE*)&RITMO, sizeof(DWORD));
					RegCloseKey(hKey);
				}

				_tprintf(_T("New speed (RYTHM): %d ms\n"), RITMO);
			}
		}
		else {
			_tprintf(_T("Unknown command. Type '?' for help.\n"));
		}

		ReleaseMutex(hMutex);
	}
}

DWORD WINAPI clientHandler(LPVOID lpParam) {
	PipeClientContext* ctx = (PipeClientContext*)lpParam;
	HANDLE hPipe = ctx->pipe;
	GameControlData* cdata = ctx->cdata;

	GAME_MESSAGE msg;
	DWORD bytesR;

	while (ReadFile(hPipe, &msg, sizeof(GAME_MESSAGE), &bytesR, NULL) && bytesR > 0) {
		WaitForSingleObject(cdata->hGameMutex, INFINITE);

		GAME_MESSAGE response;
		_tcscpy_s(response.sender, MAX_NAME_LENGTH, _T("SERVER"));
		response.msgType = MSG_RESPONSE;
		_tcscpy_s(response.content, 256, _T(""));

		switch (msg.msgType) {
		case MSG_REGISTER: {
			int idx = cdata->sharedMem->playerCount++;
			_tcscpy_s(cdata->sharedMem->playerList[idx].name, MAX_NAME_LENGTH, msg.sender);
			cdata->sharedMem->playerList[idx].points = 0;
			cdata->sharedMem->playerList[idx].active = 1;
			cdata->sharedMem->playerList[idx].hPipe = hPipe;
			_stprintf_s(response.content, 256, _T("Welcome %s!"), msg.sender);

			int activePlayers = 0;
			for (int i = 0; i < cdata->sharedMem->playerCount; i++) {
				if (cdata->sharedMem->playerList[i].active) {
					activePlayers++;
				}
			}

			if (activePlayers >= 2) {
				cdata->sharedMem->running = 1;
			}

			break;
		}
		case MSG_COMMAND: {
			if (_tcscmp(msg.content, _T(":pont")) == 0) {
				for (int i = 0; i < cdata->sharedMem->playerCount; i++) {
					if (_tcscmp(cdata->sharedMem->playerList[i].name, msg.sender) == 0) {
						_stprintf_s(response.content, 256, _T("Your current score is %d."),
							cdata->sharedMem->playerList[i].points);
						break;
					}
				}
			}
			else if (_tcscmp(msg.content, _T(":jogs")) == 0) {
				TCHAR buffer[256] = _T("Leaderboard:\n");
				for (int i = 0; i < cdata->sharedMem->playerCount; i++) {
					if (cdata->sharedMem->playerList[i].active) {
						TCHAR line[64];
						_stprintf_s(line, 64, _T("- %s: %d pts\n"),
							cdata->sharedMem->playerList[i].name,
							cdata->sharedMem->playerList[i].points);
						_tcscat_s(buffer, 256, line);
					}
				}
				_tcscpy_s(response.content, 256, buffer);
			}
			else {
				_tcscpy_s(response.content, 256, _T("Unknown command."));
			}
			break;
		}
		case MSG_DISCONNECT: {
			for (int i = 0; i < cdata->sharedMem->playerCount; i++) {
				if (_tcscmp(cdata->sharedMem->playerList[i].name, msg.sender) == 0) {
					cdata->sharedMem->playerList[i].active = 0;
					cdata->sharedMem->playerList[i].hPipe = NULL;
					break;
				}
			}
		   
			_tcscpy_s(response.content, 256, _T("Disconnected successfully."));
			GAME_MESSAGE response_copy = response;

			int activePlayers = 0;
			for (int i = 0; i < cdata->sharedMem->playerCount; i++) {
				if (cdata->sharedMem->playerList[i].active) {
					activePlayers++;
				}
			}

			if (activePlayers <= 1 && cdata->sharedMem->running == 1) {
				_tprintf(_T("Not enough players to continue game!!"));
				cdata->sharedMem->running = 0;
			}

			ReleaseMutex(cdata->hGameMutex);

			WriteFile(hPipe, &response_copy, sizeof(GAME_MESSAGE), &bytesR, NULL);
			break;
		}
		case MSG_GUESS:
		{
			int pointsGiven = 0;
			int valid = validateGuess(msg.content);
			if (valid)
			{
				for (int i = 0; i < cdata->sharedMem->playerCount; i++)
				{
					if (_tcscmp(cdata->sharedMem->playerList[i].name, msg.sender) == 0) {
						pointsGiven = _tcslen(msg.content) * 1.5;
						cdata->sharedMem->playerList[i].points += pointsGiven;
						break;
					}
				}
			}
			else if (!valid)
			{
				for (int i = 0; i < cdata->sharedMem->playerCount; i++)
				{
					if (_tcscmp(cdata->sharedMem->playerList[i].name, msg.sender) == 0) {
						pointsGiven = _tcslen(msg.content) * 0.5;
						if (cdata->sharedMem->playerList[i].points < pointsGiven)
						{
							cdata->sharedMem->playerList[i].points = 0;
						}
						else
						{
							cdata->sharedMem->playerList[i].points -= pointsGiven;

						}
						break;
					}
				}
			}
			response.msgType = MSG_RESPONSE;
			if (valid)
			{
				_stprintf_s(response.content, 256, _T(""));
				TCHAR broadcastMsg[256];
				_stprintf_s(broadcastMsg, 256, _T("User %s guessed correctly and was awarded %d points. Word: %s!"), msg.sender, pointsGiven, msg.content);
				broadcastMaker(cdata, broadcastMsg);
			}
			else if (!valid)
			{
				_stprintf_s(response.content, 256, _T("\nYour guess was incorrect"), msg.content, valid);
			}
			pointsGiven = 0;
			break;
		}
		}

		GAME_MESSAGE response_copy = response;

		ReleaseMutex(cdata->hGameMutex);

		WriteFile(hPipe, &response_copy, sizeof(GAME_MESSAGE), &bytesR, NULL);
	}

	CloseHandle(hPipe);
	free(ctx);
	return 0;
}

DWORD WINAPI pipeCreator(LPVOID pArguments) {
	GameControlData* cdata = (GameControlData*)pArguments;

	while (1) {
		HANDLE hPipe = CreateNamedPipe(
			PIPE_NAME,
			PIPE_ACCESS_DUPLEX,
			PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
			MAX_PLAYERS,
			sizeof(GAME_MESSAGE),
			sizeof(GAME_MESSAGE),
			0,
			NULL
		);

		if (hPipe == INVALID_HANDLE_VALUE) {
			_tprintf(_T("Error creating named pipe: %d\n"), GetLastError());
			continue;
		}

		BOOL connected = ConnectNamedPipe(hPipe, NULL) || GetLastError() == ERROR_PIPE_CONNECTED;
		if (connected) {
			PipeClientContext* ctx = (PipeClientContext*)malloc(sizeof(PipeClientContext));
			ctx->pipe = hPipe;
			ctx->cdata = cdata;
			CreateThread(NULL, 0, clientHandler, ctx, 0, NULL);
		}
		else {
			CloseHandle(hPipe);
		}
	}
	return 0;
}




int _tmain(int argc, TCHAR* argv[]) {
#ifdef UNICODE
	_setmode(_fileno(stdin), _O_WTEXT);
	_setmode(_fileno(stdout), _O_WTEXT);
	_setmode(_fileno(stderr), _O_WTEXT);
#endif


	if (!LoadOrCreateRegistrySettings()) {
		_tprintf(_T("Usando valores padrão: MAXLETRAS = %d, RITMO = %d\n"), MAXLETRAS, RITMO);
	}

	GameControlData cdata;
	if (!initResources(&cdata)) {
		_tprintf(_T("Failed to initialize resources\n"));
		return 1;
	}

	hMutex = CreateMutex(NULL, FALSE, NULL);
	if (hMutex == NULL) {
		_tprintf(_T("Error creating mutex\n"));
		return 1;
	}

	hSemaphore = CreateSemaphore(NULL, 0, BUFFER_SIZE, NULL);
	if (hSemaphore == NULL) {
		_tprintf(_T("Error creating semaphore\n"));
		CloseHandle(hMutex);
		return 1;
	}

	HANDLE hThreads[2];
	unsigned threadID;

	hThreads[0] = (HANDLE)_beginthreadex(NULL, 0, &GeraLetrasThread, &cdata, 0, &threadID);
	hThreads[1] = (HANDLE)_beginthreadex(NULL, 0, &GuessValidationThread, NULL, 0, &threadID);

	HANDLE hPipe = CreateThread(NULL, 0, pipeCreator, &cdata, 0, NULL);

	TCHAR cmd[100];
	while (1) {
		_tprintf(_T("\n <Overseer> \n - Type '?' to see available commands\n"));
		_fgetts(cmd, 100, stdin);
		cmd[_tcslen(cmd) - 1] = '\0';

		if (_tcscmp(cmd, _T("exit")) == 0) {
			break;
		}
		else if (_tcscmp(cmd, _T("show mem")) == 0) {
			displaySharedMemory(&cdata);
		}
		else {
			processCommand(cmd, &cdata);
		}
	}

	_tprintf(_T("\nOverseer shutdown initiated...\n"));
	cdata.shouldContinue = 0;

	WaitForMultipleObjects(2, hThreads, TRUE, INFINITE);

	for (int i = 0; i < 2; i++) {
		CloseHandle(hThreads[i]);
	}

	CloseHandle(hMutex);
	CloseHandle(hSemaphore);

	UnmapViewOfFile(cdata.sharedMem);
	CloseHandle(cdata.hMapFile);
	CloseHandle(cdata.hGameMutex);
	CloseHandle(cdata.hCommandMutex);
	CloseHandle(cdata.hPlayerSemaphore);
	CloseHandle(cdata.hLetterSemaphore);

	return 0;
}