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

TCHAR dictionary[1236][MAX_WORD_LENGTH];
PLAYER playerList[10];
int playerCount = 0;

/**
 * LoadOrCreateRegistrySettings - Acho que mais obvio o nome não pode ser, vai tentar ir buscar a info do RITMO e MAXLETRAS a registry, se não existir cria esse par-valor
 *
 * 
 * 
 * @return  True - Correu tudo bem, o par chave-valor foi criado ou lido com sucesso
 *          False - GG, algo correu mal, vai usar os valores definidos como RITMO e MAXLETRAS
 */
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

/** initResources - Inicializa os recursos principais, como a shared memory e cenas da sincronização
 * 
 * @param cdata - Genuinamente queria encontrar uma maneira mais facil de chamar este parametro, talvez
 *                definir cdata como global, mas acho q isso dava merda, por isso gramamos com passar *cdata
 *                constantemente
 * @return True - Está tudo fixe, se não, o programa fecha logo
 *         False - Alguma cena pifou, nao percebo como, mas de qualquer maneira, o programa fecha
 */
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
    cdata->hStateMutex = CreateMutex(NULL, FALSE, STATE_MUTEX_NAME);

    cdata->hStartEvent = CreateEvent(NULL, TRUE, FALSE, NULL); 

    WaitForSingleObject(cdata->hGameMutex, INFINITE);
    memset(cdata->sharedMem->letters, '_', MAXLETRAS);
    memset(cdata->sharedMem->displayedLetters, '_', MAXLETRAS);
    memset(cdata->sharedMem->letterInfo, 0, sizeof(int) * MAXLETRAS * 2);
    cdata->sharedMem->playerCount = 0;
    cdata->sharedMem->running = 0;
    cdata->sharedMem->currentLeaderPoints = 0;
    _tcscpy_s(cdata->sharedMem->currentLeader, MAX_NAME_LENGTH, _T(""));
    ReleaseMutex(cdata->hGameMutex);

    cdata->shouldContinue = 0;

    return TRUE;
}

/** LoadDictionaryFromFile - Nomes mais explicativos não existem, so carrega o dicionario do ficheiro.txt,
 *                           nem precisava de passar o nome do ficheiro porque é sempre o mesmo, mas eu até
 *                           curto de escrever argumentos em funções. Provavelmente quando formos testar na
 *                           defesa vai dar merda porque o caminho do ficheiro é diferente, mas isso é um problema
 *                           para outro dia.
 * 
 * @param filePath         - Caminho do dicionário
 * @param dictionary       - Array onde o dicionário vai ser guardado, porque é q simplesmente não usei uma variavel global? Eu usei e como sou porreiro vou deixar assim na mesma
 *                           , pode ser preciso aceder ao dicionario noutra função ou num futuro distante
 * @param wordCount        - Ponteiro para o número de palavras lidas, porque é que não usei uma variavel global? Porque sou porreiro e gosto de passar argumentos
 * @return 
 */
BOOL LoadDictionaryFromFile(const TCHAR* filePath, TCHAR dictionary[][MAX_WORD_LENGTH], int* wordCount) {
    FILE* file;
    _tfopen_s(&file, filePath, _T("r"));
    if (file == NULL) {
        _tprintf(_T("Failed to open dictionary file: %s\n"), filePath);
        return FALSE;
    }

    int count = 0;
    while (_fgetts(dictionary[count], MAX_WORD_LENGTH, file) != NULL) {
        size_t len = _tcslen(dictionary[count]);
        if (len > 0 && dictionary[count][len - 1] == '\n') {
            dictionary[count][len - 1] = '\0';
        }
        count++;

        if (count >= 1236) {
            break;
        }
    }

    *wordCount = count;
    fclose(file);

    _tprintf(_T("Dictionary loaded successfully with %d words.\n"), count);
    return TRUE;
}


/** printABCR - Imprime o array abcR, ja fizemos isto a tanto tempo que ja nao me lembro
 *                da diferença entre isto e o abcRDisplay.
 */
void printABCR() {
    _tprintf(_T("\n\nabcR (Oldest comes first): "));
    for (int i = 0; i < MAXLETRAS; i++) {
        _tprintf(_T("%c "), abcR[i]);
    }
    _tprintf(_T("\n"));
}

/** printABCRDisplay - Nem te vais acreditar no que esta função faz...
 *                
 */

void printABCRDisplay() {
    _tprintf(_T("\n\nabcRDisplay (What players see): "));
    for (int i = 0; i < MAXLETRAS; i++) {
        _tprintf(_T("%c "), abcRDisplay[i]);
    }
    _tprintf(_T("\n"));
}

/** createLetter - Escolhe uma letra aleatoria e coloca-a na posição indicada
 * 
 * @param pos - Nunca vais adivinhar, é a posição onde a letra vai ser colocada
 */
void createLetter(int pos) {
    _tprintf(_T("[DEBUG] createLetter called for pos %d\n"), pos);
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

/** removeOldest - Remove a letra mais antiga do array abcR, ou seja, a letra mais a esquerda
 * 
 */
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

/** copyData - Copia os dados do abcR, abcRDisplay e abcRInfo para a shared memory
 *             , nunca tive tão feliz com uma função, poupou tanto trabalho
 *             
 * @param cdata - Estou farto de ver cdata a frente, mas é o que temos
 */
void copyData(GameControlData* cdata)
{
    memcpy(cdata->sharedMem->displayedLetters, abcRDisplay, sizeof(abcRDisplay));
    memcpy(cdata->sharedMem->letters, abcR, sizeof(abcR));
    memcpy(cdata->sharedMem->letterInfo, abcRInfo, sizeof(abcRInfo));
}

/** toggleGame - Função schizo que crier enquanto tentatava perceber o erro do broadcast, basicamente so
 *               ve se tem jogadores sufecientes para começar o jogo ou não, se sim, começa o jogo
 * 
 * @param cdata - EU ESTOU TÂO FARTO DE TI
 * @return GAME_STATE_NO_CHANGE - Não houve mudança nenhuma (ou seja, se estava ativo, continua ativo, se estava inativo, continua inativo)
 *         GAME_STATE_STARTED - O jogo começou, ou seja, tem 2 jogadores
 *         GAME_STATE_PAUSED - O jogo parou, ou seja, tem menos de 2 jogadores
 */
int toggleGame(GameControlData* cdata) {
    int result = GAME_STATE_NO_CHANGE;

    WaitForSingleObject(cdata->hStateMutex, INFINITE);

    if (cdata->sharedMem->playerCount >= 2 && !cdata->shouldContinue) {
        cdata->shouldContinue = 1;
        SetEvent(cdata->hStartEvent);
        cdata->sharedMem->running = 1;
        result = GAME_STATE_STARTED;
    }
    else if (cdata->sharedMem->playerCount < 2 && cdata->shouldContinue) {
        cdata->shouldContinue = 0;
        ResetEvent(cdata->hStartEvent);
        cdata->sharedMem->running = 0;
        result = GAME_STATE_PAUSED;
    }

    ReleaseMutex(cdata->hStateMutex);
    return result;
}


/** GeraLetrasThread - Quem ler o nome vai achar que é uma thread que gera letras, quer ler o codigo vê que é isso que ela faz
 * 
 * @param pArguments - Ponteiro para os argumentos passados para a thread, neste caso é o cdata, mais uma vez
 * @return Por agora as threads não acabam gracefully, mas se calhar um dia conseguimos implementar isso, aposto que é so trocar o while
 */
DWORD WINAPI GeraLetrasThread(LPVOID pArguments) {
    _tprintf(_T("[GeraLetrasThread] Started\n"));
    GameControlData* cdata = (GameControlData*)pArguments;
    int cont = 0;
    int a = 1;
    while (1) {

        WaitForSingleObject(cdata->hStartEvent, INFINITE);


        if (a)
        {
            Sleep(RITMO);
            WaitForSingleObject(cdata->hGameMutex, INFINITE);


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


            cont = 0;
            ReleaseSemaphore(cdata->hLetterSemaphore, 1, NULL);
            ReleaseMutex(cdata->hGameMutex);
        }
        else
        {
            Sleep(500);
        }

    }
    return 0;

}

/** countLetters - Conta as letras que existem no array abcR, e imprime quantas existem
 * 
 * @param arr   -  Isto é sempre o abcR, porque é que temos isto sequer? Porgue gostas de complicar a vida
 * @param count -  Array onde vai guardar o número de letras, ou seja, o número de vezes que cada letra aparece
 */
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

/** validateGuess - Valida a palavra dada pelo jogador, ou seja, verifica se a palavra existe no dicionário e se as letras estão disponíveis
 *                  , também substitui as letras que foram adivinhadas no abcRDisplay
 *
 * @param guess - Palavra a validar
 * @return 1 - Palavra válida
 *         0 - Palavra inválida
 */
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

    for (int i = 0; i < 1236; i++) {
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

/** GuessValidationThread - Thread que valida as palavras dadas pelos jogadores, ou seja, verifica se a palavra existe no dicionário e se as letras estão disponíveis
 *
 * @param pArguments - Ponteiro para os argumentos passados para a thread, neste caso é o cdata, mais uma vez
 * @return Por agora as threads não acabam gracefully, mas se calhar um dia conseguimos implementar isso, aposto que é so trocar o while
 */
DWORD WINAPI GuessValidationThread(LPVOID pArguments) {
    GameControlData* cdata = (GameControlData*)pArguments;
    while (1) {
        if (cdata->shouldContinue)
        {
            WaitForSingleObject(hSemaphore, INFINITE);

            WaitForSingleObject(hMutex, INFINITE);

            TCHAR currentWord[MAX_WORD_LENGTH];
            _tcscpy_s(currentWord, MAX_WORD_LENGTH, wordBuffer[bufferStart]);
            bufferStart = (bufferStart + 1) % BUFFER_SIZE;

            ReleaseMutex(hMutex);

            validateGuess(currentWord);
        }
        else
        {
            Sleep(500);
        }
    }
    return 0;
}

// Não vou estar a documentar estas aqui.
void showGeneralHelp() {
    _tprintf(_T("\nAvailable commands:\n"));
    _tprintf(_T("? - Show this help\n"));
    _tprintf(_T("kick <player> - Remove a player\n"));
    _tprintf(_T("show ? - Show display options\n"));
    _tprintf(_T("bot ? - Show bot commands\n"));
    _tprintf(_T("rythm ? - Show rythm commands\n"));
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

void showRythmHelp() {
    _tprintf(_T("\nRythm commands:\n"));
    _tprintf(_T("rythm up - Increase the speed (reduce interval by 1000 ms)\n"));
    _tprintf(_T("rythm down - Decrease the speed (increase interval by 1000 ms)\n"));
}

/** displaySharedMemory - Imprime o conteudo da shared memory, ou seja, as letras e o estado do jogo
 * 
 * @param cdata - EU ODEIO-TE TANTO CDATA
 */
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

/** broadcastMaker - Nunca tive tanta raiva de uma função, basicamente faz broadcast a todos os jogadores, quando lhe apetece
 *                   pelo que me apercebi.
 * 
 * @param cdata - ...
 * @param messageContent - A frase que é para dar broadcast
 */
void broadcastMaker(GameControlData* cdata, const TCHAR* messageContent)
{
    WaitForSingleObject(cdata->hGameMutex, INFINITE);

    GAME_MESSAGE broadcast;
    broadcast.msgType = MSG_BROADCAST;
    _tcscpy_s(broadcast.sender, MAX_NAME_LENGTH, _T("SERVER"));
    _tcscpy_s(broadcast.content, 256, messageContent);

    for (int i = 0; i < cdata->sharedMem->playerCount; i++) {
        if (cdata->sharedMem->playerList[i].active && cdata->sharedMem->playerList[i].hPipe != NULL) {
            OVERLAPPED ov = { 0 };
            ov.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

            DWORD written;
            if (!WriteFile(cdata->sharedMem->playerList[i].hPipe,
                &broadcast,
                sizeof(GAME_MESSAGE),
                &written,
                &ov)) {
                if (GetLastError() == ERROR_IO_PENDING) {
                    WaitForSingleObject(ov.hEvent, INFINITE);
                }
                else {
                    _tprintf(_T("Error broadcasting to player %s: %d\n"),
                        cdata->sharedMem->playerList[i].name,
                        GetLastError());
                }
            }
            CloseHandle(ov.hEvent);
        }
    }
    _tprintf(_T("\n[Broadcast] %s\n"), messageContent);

}

/** LaunchBot - Lança o bot, ou seja, cria um processo novo com o bot.exe e passa-lhe os argumentos, porque é que é a unica função
 *              que começa por uma letra maiuscula? 
 * 
 * @param username - Nome do bot, ou seja, o nome que vai aparecer no jogo
 * @param reactionTime - Tempo de reação do bot, ou seja, o tempo que o bot vai esperar para adivinhar a palavra
 * @param cdata - uh...
 * @return True - O bot foi criado com sucesso
 *         False - O bot não foi criado com sucesso
 */
BOOL LaunchBot(TCHAR* username, int reactionTime, GameControlData* cdata) {
    TCHAR commandLine[256];
    _stprintf_s(commandLine, 256, _T("bot.exe %s %d"), username, reactionTime);

    STARTUPINFO si = { 0 };
    PROCESS_INFORMATION pi = { 0 };
    si.cb = sizeof(STARTUPINFO);

    BOOL success = CreateProcess(
        _T("C:\\Users\\fakyc\\source\\repos\\TrabalhoPratico_SistemasOperativos2\\x64\\Debug\\Bot.exe"),
        commandLine,
        NULL,
        NULL,
        FALSE,
        CREATE_NEW_CONSOLE,
        NULL,
        _T("C:\\Users\\fakyc\\source\\repos\\TrabalhoPratico_SistemasOperativos2\\x64\\Debug"),
        &si, &pi
    );

    if (!success) {
        _tprintf(_T("Failed to create bot process. Error: %d\n"), GetLastError());
        return;
    }


    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return TRUE;
}

/** processCommand - Processa os comandos dados pelo administrador
 *
 * @param command - Comando dado pelo administrador
 * @param cdata - ... uhhh
 */
void processCommand(TCHAR* command, GameControlData* cdata) {
    TCHAR cmd1[20], cmd2[20], cmd3[20];
    TCHAR param[20];

    if (_stscanf_s(command, _T("%19s %19s %19s"),
        cmd1, (unsigned)_countof(cmd1),
        cmd2, (unsigned)_countof(cmd2),
        param, (unsigned)_countof(param)) >= 1) {

        WaitForSingleObject(hMutex, INFINITE);

        if (_tcscmp(cmd1, _T("?")) == 0) {
            showGeneralHelp();
        }
        else if (_tcscmp(cmd1, _T("show")) == 0 && _tcscmp(cmd2, _T("?")) == 0) {
            showDisplayHelp();
        }
        else if (_tcscmp(cmd1, _T("rythm")) == 0 && _tcscmp(cmd2, _T("?")) == 0)
        {
            showRythmHelp();
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
                _tprintf(_T("\n=== Connected Players ===\n"));
                int activePlayers = 0;

                for (int i = 0; i < cdata->sharedMem->playerCount; i++) {
                    if (cdata->sharedMem->playerList[i].active) {
                        _tprintf(_T("%d. %s - %d points\n"),
                            activePlayers + 1,
                            cdata->sharedMem->playerList[i].name,
                            cdata->sharedMem->playerList[i].points);
                        activePlayers++;
                    }
                }

                if (activePlayers == 0) {
                    _tprintf(_T("No players currently connected.\n"));
                }
                else {
                    _tprintf(_T("\nTotal connected players: %d\n"), activePlayers);
                }
            }
        }
        else if (_tcscmp(cmd1, _T("bot")) == 0) {
            if (_tcscmp(cmd2, _T("create")) == 0) {

                TCHAR username[32] = { 0 };
                int reactionTime = 0;
                param[_countof(param) - 1] = '\0';


                if (_stscanf_s(command, _T("%*s %*s %31s %d"), username, (unsigned)_countof(username), &reactionTime) != 2) {
                    _tprintf(_T("Invalid parameters. Usage: bot create <username> <reaction_time_ms>.\n"));
                    ReleaseMutex(hMutex);
                    return;
                }

                if (reactionTime < 5000) {
                    _tprintf(_T("Reaction time must be greater or equal than 5000.\n"));
                    ReleaseMutex(hMutex);
                    return;
                }


                BOOL usernameTaken = FALSE;
                for (int i = 0; i < cdata->sharedMem->playerCount; i++) {
                    if (_tcscmp(cdata->sharedMem->playerList[i].name, username) == 0) {
                        usernameTaken = TRUE;
                        break;
                    }
                }

                if (usernameTaken) {
                    _tprintf(_T("Username '%s' is already in use. Choose a different name.\n"), username);
                    ReleaseMutex(hMutex);
                    return;
                }

                if (LaunchBot(username, reactionTime, cdata)) {
                    _tprintf(_T("Bot '%s' launched successfully with reaction time %d ms.\n"), username, reactionTime);

                }
                else {
                    _tprintf(_T("Failed to launch bot '%s'.\n"), username);
                }

            }
            else if (_tcscmp(cmd2, _T("remove")) == 0) {
                if (_tcslen(param) == 0) {
                    _tprintf(_T("Usage: bot remove <name>\n"));
                    ReleaseMutex(hMutex);
                    return;
                }
                BOOL found = FALSE;
                for (int i = 0; i < cdata->sharedMem->playerCount; i++) {
                    if (cdata->sharedMem->playerList[i].active &&
                        _tcscmp(cdata->sharedMem->playerList[i].name, param) == 0) {

                        _tprintf(_T("Removing bot: %s\n"), param);

                        if (cdata->sharedMem->playerList[i].hPipe != NULL) {
                            GAME_MESSAGE kickMsg;
                            kickMsg.msgType = MSG_KICK;
                            _tcscpy_s(kickMsg.sender, MAX_NAME_LENGTH, _T("SERVER"));
                            _tcscpy_s(kickMsg.content, 256, _T("You have been removed by the overseer."));

                            DWORD written;
                            WriteFile(cdata->sharedMem->playerList[i].hPipe, &kickMsg, sizeof(GAME_MESSAGE), &written, NULL);
                            FlushFileBuffers(cdata->sharedMem->playerList[i].hPipe);

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

                        _tprintf(_T("Bot '%s' removed successfully.\n"), param);

                        TCHAR msg[256];
                        _stprintf_s(msg, 256, _T("Bot %s has been removed from the game."), param);
                        broadcastMaker(cdata, msg);

                        found = TRUE;
                        break;
                    }
                }

                if (!found) {
                    _tprintf(_T("No bot or player named '%s' was found.\n"), param);
                }
            }
        }
        else if (_tcscmp(cmd1, _T("rythm")) == 0)
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
            }
            else if (_tcscmp(cmd2, _T("down")) == 0)
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
/** clientHandler - Função que lida com os clientes, ou seja, recebe mensagens dos clientes e processa-as
 *
 * @param lpParam - Ponteiro para os argumentos passados para a thread, neste caso é o cdata, mais uma vez
 * @return 0 - Supostamente da return 0 quando acaba, mas a este andar isto nunca na vida acontece
 */
DWORD WINAPI clientHandler(LPVOID lpParam) {
    PipeClientContext* ctx = (PipeClientContext*)lpParam;
    HANDLE hPipe = ctx->pipe;
    GameControlData* cdata = ctx->cdata;

    GAME_MESSAGE msg;
    DWORD bytesR;

    while (ReadFile(hPipe, &msg, sizeof(GAME_MESSAGE), &bytesR, NULL) && bytesR > 0) {

        GAME_MESSAGE response;
        _tcscpy_s(response.sender, MAX_NAME_LENGTH, _T("SERVER"));
        response.msgType = MSG_RESPONSE;
        _tcscpy_s(response.content, 256, _T(""));

        switch (msg.msgType) {
        case MSG_REGISTER: {
            BOOL usernameTaken = FALSE;
            WaitForSingleObject(cdata->hGameMutex, INFINITE);
            for (int i = 0; i < cdata->sharedMem->playerCount; i++) {
                if (_tcscmp(cdata->sharedMem->playerList[i].name, msg.sender) == 0) {
                    usernameTaken = TRUE;
                    break;
                }
            }

            if (usernameTaken)
            {
                GAME_MESSAGE response;
                response.msgType = MSG_KICK;
                _tcscpy_s(response.sender, MAX_NAME_LENGTH, _T("SERVER"));
                _tcscpy_s(response.content, 256, _T("Username already taken. Retry with a different username"));


                ReleaseMutex(cdata->hGameMutex);
                DWORD bytesW;
                WriteFile(hPipe, &response, sizeof(GAME_MESSAGE), &bytesW, NULL);

                CloseHandle(hPipe);
                free(ctx);
                return 0;
            }


            int idx = cdata->sharedMem->playerCount++;
            _tcscpy_s(cdata->sharedMem->playerList[idx].name, MAX_NAME_LENGTH, msg.sender);
            cdata->sharedMem->playerList[idx].points = 0;
            cdata->sharedMem->playerList[idx].active = 1;
            cdata->sharedMem->playerList[idx].hPipe = hPipe;
            _stprintf_s(response.content, 256, _T("Welcome %s!"), msg.sender);
            TCHAR braod[256];
            _stprintf_s(braod, 256, _T("User %s connected to the game!"), msg.sender);
            broadcastMaker(cdata, braod);

            int change = toggleGame(cdata);
            if (change == GAME_STATE_STARTED) {
                // Por muito que eu gostasse de perceber oq está a causar o pipe do primeiro jogador crashar
                // ate que o segundo jogador dé input a alguma coisa, eu genuinamente não consigo ver, os mutexes
                // estão todos bem, nada está a ser acedido onde não devia, os pipes deviam estar limpos sem problemas
                // mas mesmo assim eu esta merda de broadcast não funciona e eu estou farto de tentar arranjar isto, vai assim.
               // broadcastMaker(cdata, _T("GAME STARTED: Enough players are connected!"));
            }
            else if (change == GAME_STATE_PAUSED) {
               // broadcastMaker(cdata, _T("GAME PAUSED: Not enough players to continue."));
            }
       
            ReleaseMutex(cdata->hGameMutex);
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
                PLAYER sorted[MAX_PLAYERS];
                int count = 0;

                for (int i = 0; i < cdata->sharedMem->playerCount; i++) {
                    if (cdata->sharedMem->playerList[i].active) {
                        sorted[count++] = cdata->sharedMem->playerList[i];
                    }
                }

                for (int i = 0; i < count - 1; i++) {
                    for (int j = i + 1; j < count; j++) {
                        if (sorted[j].points > sorted[i].points) {
                            PLAYER temp = sorted[i];
                            sorted[i] = sorted[j];
                            sorted[j] = temp;
                        }
                    }
                }

                TCHAR buffer[256] = _T("Leaderboard:\n");
                for (int i = 0; i < count; i++) {
                    TCHAR line[64];
                    _stprintf_s(line, 64, _T("%d. %s - %d pts\n"),
                        i + 1, sorted[i].name, sorted[i].points);
                    _tcscat_s(buffer, 256, line);
                }

                _tcscpy_s(response.content, 256, buffer);
            }

            else {
                _tcscpy_s(response.content, 256, _T("Unknown command."));
            }
            break;
        }
        case MSG_DISCONNECT: {
            int playerIndex = -1;

            for (int i = 0; i < cdata->sharedMem->playerCount; i++) {
                if (_tcscmp(cdata->sharedMem->playerList[i].name, msg.sender) == 0) {
                    cdata->sharedMem->playerList[i].active = 0;
                    cdata->sharedMem->playerList[i].hPipe = NULL;
                    break;
                }
            }

            int change = toggleGame(cdata);


            _tcscpy_s(response.content, 256, _T("Disconnected successfully."));
            TCHAR broadcastMsg[256];
            _stprintf_s(broadcastMsg, 256, _T("User %s has left the game!"), msg.sender);
            broadcastMaker(cdata, broadcastMsg);
            GAME_MESSAGE response_copy = response;

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
                        pointsGiven = _tcslen(msg.content);
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
                _stprintf_s(response.content, 256, _T("\nYour guess was incorrect"));
            }
            for (int j = 0; j < cdata->sharedMem->playerCount; j++)
            {
                if (cdata->sharedMem->currentLeaderPoints < cdata->sharedMem->playerList[j].points) {
                    if (_tcscmp(cdata->sharedMem->currentLeader, msg.sender) != 0) {
                        _tcscpy_s(cdata->sharedMem->currentLeader, MAX_NAME_LENGTH, msg.sender);
                        cdata->sharedMem->currentLeaderPoints = cdata->sharedMem->playerList[j].points;

                        TCHAR leaderMsg[256];
                        _stprintf_s(leaderMsg, 256, _T("User  %s is now leading with %d points!"),
                            msg.sender, cdata->sharedMem->playerList[j].points);
                        broadcastMaker(cdata, leaderMsg);
                        break;
                    }
                }
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

/** pipeCreator - Função que cria o pipe por utilizador
 *
 * @param pArguments - Ponteiro para os argumentos passados para a thread, neste caso é o cdata, mais uma vez
 * @return 0 - Supostamente da return 0 quando acaba, mas a este andar isto nunca na vida acontece
 */
DWORD WINAPI pipeCreator(LPVOID pArguments) {
    GameControlData* cdata = (GameControlData*)pArguments;

    while (1) {
        HANDLE hPipe = CreateNamedPipe(
            PIPE_NAME,
            PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
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
            _tprintf(_T("[Overseer] Pipe connected successfully.\n"));
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



/**
 * No main não ha muito para comentar, mas so quero deixar aqui o meu odio a semaphores
 * , porque é que existem race conditions, porque é que as threads não são inteligentes e
 *  tratam disso sozinhas meu.
 */
int _tmain(int argc, TCHAR* argv[]) {
#ifdef UNICODE
    _setmode(_fileno(stdin), _O_WTEXT);
    _setmode(_fileno(stdout), _O_WTEXT);
    _setmode(_fileno(stderr), _O_WTEXT);
#endif
    HANDLE hSingleInstanceMutex = CreateMutex(NULL, TRUE, _T("Global\\OverseerSingleInstanceMutex"));
    DWORD lastError = GetLastError();

    if (hSingleInstanceMutex == NULL) {
        _tprintf(_T("Failed to create single instance mutex. Error: %d\n"), GetLastError());
        return 1;
    }

    if (lastError == ERROR_ALREADY_EXISTS) {
        _tprintf(_T("Another instance of Overseer is already running.\nOnly one instance is allowed at a time.\n"));
        CloseHandle(hSingleInstanceMutex);
        return 1;
    }

    if (!LoadOrCreateRegistrySettings()) {
        _tprintf(_T("Usando valores padrão: MAXLETRAS = %d, RITMO = %d\n"), MAXLETRAS, RITMO);
    }

    int wordCount = 0;
    if (!LoadDictionaryFromFile(_T("C:\\Users\\fakyc\\source\\repos\\TrabalhoPratico_SistemasOperativos2\\x64\\Debug\\dictionary_small.txt"), dictionary, &wordCount)) {
        _tprintf(_T("Failed to load dictionary. Exiting...\n"));
        return 1;
    }

    GameControlData cdata;
    if (!initResources(&cdata)) {
        _tprintf(_T("Failed to initialize resources\n"));
        return 1;
    }
    cdata.shouldContinue = 0;
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

    HANDLE hThreads[3];
    unsigned threadID;

    hThreads[0] = CreateThread(NULL, 0, GeraLetrasThread, &cdata, 0, NULL);
    hThreads[1] = CreateThread(NULL, 0, GuessValidationThread, &cdata, 0, NULL);
    hThreads[2] = CreateThread(NULL, 0, pipeCreator, &cdata, 0, NULL);

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

    TCHAR shutdownMsg[256];
    _stprintf_s(shutdownMsg, 256, _T("SERVER SHUTDOWN: The game server is shutting down."));
    broadcastMaker(&cdata, shutdownMsg);

    Sleep(1000);

    if (cdata.hGameMutex != NULL) {
        WaitForSingleObject(cdata.hGameMutex, INFINITE);

        for (int i = 0; i < cdata.sharedMem->playerCount; i++) {
            if (cdata.sharedMem->playerList[i].active && cdata.sharedMem->playerList[i].hPipe != NULL) {
                GAME_MESSAGE kickMsg;
                kickMsg.msgType = MSG_KICK;
                _tcscpy_s(kickMsg.sender, MAX_NAME_LENGTH, _T("SERVER"));
                _tcscpy_s(kickMsg.content, 256, _T("Server is shutting down. Connection will be closed."));

                DWORD bytesWritten;
                WriteFile(cdata.sharedMem->playerList[i].hPipe, &kickMsg, sizeof(GAME_MESSAGE), &bytesWritten, NULL);

                Sleep(200);

                FlushFileBuffers(cdata.sharedMem->playerList[i].hPipe);
                DisconnectNamedPipe(cdata.sharedMem->playerList[i].hPipe);
                CloseHandle(cdata.sharedMem->playerList[i].hPipe);
                cdata.sharedMem->playerList[i].hPipe = NULL;
            }
        }

        ReleaseMutex(cdata.hGameMutex);
    }

    DWORD waitResult = WaitForMultipleObjects(3, hThreads, TRUE, 5000);
    if (waitResult == WAIT_TIMEOUT) {
        _tprintf(_T("Threads did not gracefully end.\n"));
    }

    for (int i = 0; i < 3; i++) {
        if (hThreads[i] != NULL) {
            CloseHandle(hThreads[i]);
        }
    }

    if (cdata.sharedMem != NULL) {
        UnmapViewOfFile(cdata.sharedMem);
    }

    if (cdata.hMapFile != NULL) {
        CloseHandle(cdata.hMapFile);
    }

    if (cdata.hGameMutex != NULL) {
        CloseHandle(cdata.hGameMutex);
    }

    if (cdata.hCommandMutex != NULL) {
        CloseHandle(cdata.hCommandMutex);
    }

    if (cdata.hPlayerSemaphore != NULL) {
        CloseHandle(cdata.hPlayerSemaphore);
    }

    if (cdata.hLetterSemaphore != NULL) {
        CloseHandle(cdata.hLetterSemaphore);
    }

    if (hMutex != NULL) {
        CloseHandle(hMutex);
    }

    if (hSemaphore != NULL) {
        CloseHandle(hSemaphore);
    }
    CloseHandle(hSingleInstanceMutex);

    _tprintf(_T("Overseer shutdown complete.\n"));
    return 0;
}