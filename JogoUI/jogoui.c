/*
*Funções:
 1 -> Deve indicar ao arbitro quem esta a tentar entrar no jogo
 2 -> Se o jogador vai sair
 3 -> O jogador indicou um palavra e a respetiva palavra
 4 -> O jogador deseja saber a sua pontuacao e lista de jogadores (ou outras informacoes)
 5 -> Receber indicacoes do jogador e passar para o arbitro
 6 -> Receber indicacoes do arbitro e passar para o utilizador
 *
 *
 * Restrições:
 1 -> É lancado por cada jogador e ha apenas uma instancia por jogador.
 2 -> Nao mexe em nada que seja regras e dados de jogo. Apenas uma ponte entre arbitro e jogador.
 3 -> Comunicacao feita em consola
 *
 *
*/
#include "../Project1/utils.h"

#define LETTERS_START_Y 0
#define MENU_START_Y 5
#define INPUT_START_Y 30

HANDLE hConsole;
COORD inputPos = { 0, 0 };
HANDLE hPipe;


int MAXLETRAS = 6;
int RITMO = 3000;

/** LoadRegistrySettings - Carrega as configurações do registro, se não existirem, usa os valores padrão
 *
 * @return TRUE se tudo correr bem, FALSE se algo correr mal
 */
BOOL LoadRegistrySettings() {
    HKEY hKey;
    LONG result = RegOpenKeyEx(
        HKEY_CURRENT_USER,
        _T("Software\\TrabSO2"),
        0,
        KEY_READ,
        &hKey
    );

    if (result != ERROR_SUCCESS) {
        _tprintf(_T("Erro ao abrir chave do registro (usando valores padrão)\n"));
        return FALSE;
    }

    DWORD maxLetras = 0;
    DWORD ritmo = 0;
    DWORD size = sizeof(DWORD);

    if (RegQueryValueEx(hKey, _T("MAXLETRAS"), NULL, NULL, (LPBYTE)&maxLetras, &size) == ERROR_SUCCESS) {
        MAXLETRAS = maxLetras;
    }

    size = sizeof(DWORD);
    if (RegQueryValueEx(hKey, _T("RITMO"), NULL, NULL, (LPBYTE)&ritmo, &size) == ERROR_SUCCESS) {
        RITMO = ritmo;
    }

    RegCloseKey(hKey);

    _tprintf(_T("MAXLETRAS = %d | RITMO = %d\n"), MAXLETRAS, RITMO);

    return TRUE;
}


/** UpdateLetters - Atualiza as letras apresentadas na tela
 *
 *  A parte do SetConsoleCursorPosition permite fazer cenas bue porreiras com a consola, mas nem os gajos
 *  do windows conseguem fazer com que isto funciona bem, so está aqui para facilitar uma beca
 *
 * @param cdata - Esta merda de estrutura chamada cdata não me larga
 */
void UpdateLetters(GameControlData* cdata) {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hConsole, &csbi);

    COORD savePos = csbi.dwCursorPosition;

    SetConsoleCursorPosition(hConsole, (COORD) { 0, LETTERS_START_Y });

    for (int i = 0; i < 3; i++) {
        _tprintf(_T("                                             \n"));
    }

    SetConsoleCursorPosition(hConsole, (COORD) { 0, LETTERS_START_Y });

    WaitForSingleObject(cdata->hGameMutex, INFINITE);

    _tprintf(_T("╔═══════════════════════════════════════════╗\n"));
    _tprintf(_T("║ Available: "));
    for (int i = 0; i < MAXLETRAS; i++) {
        _tprintf(_T(" %c "), cdata->sharedMem->displayedLetters[i]);
    }
    _tprintf(_T(" ║\n"));
    _tprintf(_T("╚═══════════════════════════════════════════╝\n"));

    ReleaseMutex(cdata->hGameMutex);

    SetConsoleCursorPosition(hConsole, savePos);
}


/** LetterUpdateThread - Está sempre a ver se alguma letra mudou na shared memory, se sim, atualiza a letra
 * 
 * @param cdata - uhm cdata, mais uma vez
 * @return - Talvez um dia
 */
DWORD WINAPI LetterUpdateThread(GameControlData* cdata)
{
    HANDLE hUpdateEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, TEXT("Global\\SharedMemUpdatedEvent"));
    if (hUpdateEvent == NULL) {
        MessageBox(NULL, _T("Could not open update event"), _T("Error"), MB_OK | MB_ICONERROR);
        return 1;
    }

    TCHAR ll[MAX_MAXLETTERS] = { 0 };

    // Initial update
    WaitForSingleObject(cdata->hGameMutex, INFINITE);
    memcpy(ll, cdata->sharedMem->displayedLetters, MAXLETRAS);
    ReleaseMutex(cdata->hGameMutex);
    UpdateLetters(cdata);

    while (1)
    {
        WaitForSingleObject(hUpdateEvent, INFINITE);

        WaitForSingleObject(cdata->hGameMutex, INFINITE);
        if (memcmp(ll, cdata->sharedMem->displayedLetters, MAXLETRAS)) {
            memcpy(ll, cdata->sharedMem->displayedLetters, MAXLETRAS);
            ReleaseMutex(cdata->hGameMutex);
            UpdateLetters(cdata);
        }
        else {
            ReleaseMutex(cdata->hGameMutex);
        }

    }

    return 0;
}

/** ReceiveOverseerResponse - Recebe a resposta do arbitro, so fiz numa função porque é mais fácil
 *
 * @param response - Ponteiro para a mensagem recebida
 * @return TRUE se tudo correr bem, FALSE se algo correr mal
 */
BOOL ReceiveOverseerResponse(GAME_MESSAGE* response) {
    if (hPipe == INVALID_HANDLE_VALUE) return FALSE;

    DWORD bytesRead;
    OVERLAPPED ov = { 0 };
    ov.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    if (!ReadFile(hPipe, response, sizeof(GAME_MESSAGE), &bytesRead, &ov)) {
        if (GetLastError() == ERROR_IO_PENDING) {
            WaitForSingleObject(ov.hEvent, INFINITE);
            GetOverlappedResult(hPipe, &ov, &bytesRead, FALSE);
        }
        else {
            CloseHandle(ov.hEvent);
            return FALSE;
        }
    }
    CloseHandle(ov.hEvent);
    return (bytesRead > 0);
}

/** ConnectToGame - Conecta o jogador ao arbitro, ou seja, cria o pipe e envia a mensagem de registo
 *
 * @param name - Nome do jogador
 * @return TRUE se tudo correr bem, FALSE se algo correr mal (i.e o nome ja existir)
 */
BOOL ConnectToGame(TCHAR* name)
{
    WaitNamedPipe(PIPE_NAME, NMPWAIT_WAIT_FOREVER);

    hPipe = CreateFile(
        PIPE_NAME,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_OVERLAPPED,
        NULL
    );


    DWORD mode = PIPE_READMODE_MESSAGE | PIPE_NOWAIT;
    SetNamedPipeHandleState(hPipe, &mode, NULL, NULL);

    GAME_MESSAGE gX;
    _tcscpy_s(gX.sender, 256, name);
    gX.msgType = MSG_REGISTER;
    _tcscpy_s(gX.content, 256, _T(""));

    DWORD bytesW;
    if (!WriteFile(hPipe, &gX, sizeof(GAME_MESSAGE), &bytesW, NULL)) {
        _tprintf(_T("Registration failed\n"));
        CloseHandle(hPipe);
        return FALSE;
    }

    GAME_MESSAGE response;
    if (ReceiveOverseerResponse(&response)) {
        _tprintf(_T("%s\n"), response.content);
        return TRUE;
    }

    return FALSE;
}

/** SendMessageOverseer - Envia uma mensagem para o arbitro, ou seja, para o pipe
 *
 * @param msg - Mensagem a enviar
 * @return TRUE se tudo correr bem, FALSE se algo correr mal
 */
BOOL sendMessageOverseer(GAME_MESSAGE msg)
{

    if (hPipe == INVALID_HANDLE_VALUE)
    {
        printf("Deu erro aqui atrasado");
        return;
    }

    DWORD bytesW;
    WriteFile(hPipe, &msg, sizeof(GAME_MESSAGE), &bytesW, NULL);
    return TRUE;
}


DWORD WINAPI broadcastListener(LPVOID lpParam) {
    GAME_MESSAGE msg;
    OVERLAPPED ov = { 0 };
    ov.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    while (1) {
        DWORD bytesRead;
        BOOL success = ReadFile(hPipe, &msg, sizeof(GAME_MESSAGE), &bytesRead, &ov);

        if (!success) {
            DWORD err = GetLastError();
            if (err == ERROR_IO_PENDING) {
                WaitForSingleObject(ov.hEvent, INFINITE);
                GetOverlappedResult(hPipe, &ov, &bytesRead, FALSE);
                success = TRUE;
            }
            else if (err == ERROR_BROKEN_PIPE) {
                _tprintf(_T("\nDisconnected from server.\n"));
                CloseHandle(ov.hEvent);
                exit(0);
            }
            else {
                Sleep(100);
                continue;
            }
        }

        if (success && bytesRead > 0) {
            if (msg.msgType == MSG_BROADCAST) {
                _tprintf(_T("\n[Broadcast] %s\n"), msg.content);
               
            }
            else if (msg.msgType == MSG_RESPONSE) {
                _tprintf(_T("\n[Server] %s\n"), msg.content);
            }
            else if (msg.msgType == MSG_KICK) {
                system("cls");
                _tprintf(_T("\n[Server] %s\n"), msg.content);
                CloseHandle(ov.hEvent);
                exit(0);
            }
        }

        ResetEvent(ov.hEvent);
    }

    CloseHandle(ov.hEvent);
    return 0;
}


/*
 *
 * Honestamente acho que por agora ja temos os requisitos todos do enunciado, é so fazer testes, colocar proteções e ver ideias para o bonus
 *
*/
int _tmain(int argc, TCHAR* argv[]) {
    system("cls");
#ifdef UNICODE
    _setmode(_fileno(stdin), _O_WTEXT);
    _setmode(_fileno(stdout), _O_WTEXT);
    _setmode(_fileno(stderr), _O_WTEXT);
#endif


    if (!LoadRegistrySettings()) {
        _tprintf(_T("Using values: MAXLETRAS = %d, RITMO = %d\n"), MAXLETRAS, RITMO);
    }

    TCHAR name[32];
    _tprintf(_T("Type your username: "));
    _tscanf_s(_T("%s"), name, (unsigned)_countof(name));
    system("cls");
    ConnectToGame(name);




    hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    inputPos.X = 0;
    inputPos.Y = 10;

    GameControlData cdata;

    cdata.hMapFile = OpenFileMapping(
        FILE_MAP_ALL_ACCESS,
        FALSE,
        SHM_NAME);

    if (cdata.hMapFile == NULL) {
        _tprintf(_T("Failed to connect to overseer!\n"));
        return 1;
    }

    cdata.sharedMem = (GameSharedMem*)MapViewOfFile(
        cdata.hMapFile,
        FILE_MAP_ALL_ACCESS,
        0, 0, sizeof(GameSharedMem));

    cdata.hGameMutex = OpenMutex(
        MUTEX_ALL_ACCESS,
        FALSE,
        LETTER_MUTEX_NAME);




    HANDLE hThread = CreateThread(NULL, 0, LetterUpdateThread, &cdata, 0, NULL);
    HANDLE bThread = CreateThread(NULL, 0, broadcastListener, &cdata, 0, NULL);

    if (bThread == NULL) {
        _tprintf(_T("Failed to create broadcast listener thread!\n"));
        return 1;
    }

    TCHAR input[MAX_WORD_LENGTH] = { 0 };
    int inputLen = 0;


    while (1) {

        for (int i = 0; i < 3; i++) {
            SetConsoleCursorPosition(hConsole, (COORD) { 0, INPUT_START_Y + i });
            _tprintf(_T("                                                                                "));
        }

        SetConsoleCursorPosition(hConsole, (COORD) { 0, INPUT_START_Y });
        _tprintf(_T("\Write a guess or a command (:pont, :jogs, :sair):\n> "));

        SetConsoleCursorPosition(hConsole, (COORD) { 2, INPUT_START_Y + 2 });
        _fgetts(input, MAX_WORD_LENGTH, stdin);
        input[_tcslen(input) - 1] = '\0';

        if (_tcslen(input) == 0) continue;

        if (input[0] == ':') {
            if (_tcscmp(input, _T(":sair")) == 0) {
                GAME_MESSAGE msg;
                _tcscpy_s(msg.sender, MAX_NAME_LENGTH, name);
                msg.msgType = MSG_DISCONNECT;
                _tcscpy_s(msg.content, 256, _T(""));
                sendMessageOverseer(msg);
                CloseHandle(hPipe);
                break;
            }
            else if (_tcscmp(input, _T(":pont")) == 0) {
                GAME_MESSAGE msg;
                _tcscpy_s(msg.sender, MAX_NAME_LENGTH, name);
                msg.msgType = MSG_COMMAND;
                _tcscpy_s(msg.content, 256, _T(":pont"));
                sendMessageOverseer(msg);
            }
            else if (_tcscmp(input, _T(":jogs")) == 0) {
                GAME_MESSAGE msg;
                _tcscpy_s(msg.sender, MAX_NAME_LENGTH, name);
                msg.msgType = MSG_COMMAND;
                _tcscpy_s(msg.content, 256, _T(":jogs"));
                sendMessageOverseer(msg);
            }
            else {
                _tprintf(_T("Unknown command: %s\n"), input);
            }
        }
        else {
            GAME_MESSAGE guess;
            _tcscpy_s(guess.sender, MAX_NAME_LENGTH, name);
            _tcscpy_s(guess.content, 256, input);
            guess.msgType = MSG_GUESS;
            sendMessageOverseer(guess);
        }

        Sleep(200);
    }

    UnmapViewOfFile(cdata.sharedMem);
    CloseHandle(cdata.hMapFile);
    return 0;
}