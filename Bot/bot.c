#include "../Project1/utils.h"
int botRunning = 1;
HANDLE hPipe = INVALID_HANDLE_VALUE;



BOOL SendMessageToOverseer(GAME_MESSAGE msg) {
    if (hPipe == INVALID_HANDLE_VALUE) {
        _tprintf(_T("Pipe is invalid.\n"));
        return FALSE;
    }

    DWORD bytesWritten;
    return WriteFile(hPipe, &msg, sizeof(GAME_MESSAGE), &bytesWritten, NULL);
}

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

BOOL ConnectToGame(TCHAR* username) {
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

    if (hPipe == INVALID_HANDLE_VALUE) {
        _tprintf(_T("Failed to connect to the overseer.\n"));
        return FALSE;
    }

    DWORD mode = PIPE_READMODE_MESSAGE | PIPE_NOWAIT;
    SetNamedPipeHandleState(hPipe, &mode, NULL, NULL);

    GAME_MESSAGE msg;
    _tcscpy_s(msg.sender, MAX_NAME_LENGTH, username);
    msg.msgType = MSG_REGISTER;
    _tcscpy_s(msg.content, 256, _T(""));

    DWORD bytesWritten;
    if (!WriteFile(hPipe, &msg, sizeof(GAME_MESSAGE), &bytesWritten, NULL)) {
        _tprintf(_T("Failed to register bot.\n"));
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

DWORD WINAPI ListenToOverseer(LPVOID lpParam) {
    while (botRunning) {
        GAME_MESSAGE msg;
        if (ReceiveOverseerResponse(&msg)) {
            if (msg.msgType == MSG_KICK) {
                _tprintf(_T("[Overseer] %s\n"), msg.content);
                botRunning = 0; // Encerrar o bot
            }
            else if (msg.msgType == MSG_BROADCAST) {
                _tprintf(_T("[Broadcast] %s\n"), msg.content);
            }
        }
        Sleep(200);
    }
    return 0;
}

int _tmain(int argc, TCHAR* argv[]) {



    TCHAR username[MAX_NAME_LENGTH];
    _tcscpy_s(username, MAX_NAME_LENGTH, argv[1]);
    int reactionTime = _ttoi(argv[2]);

    if (reactionTime <= 0) {
        _tprintf(_T("Invalid reaction time. Must be greater than 0.\n"));
        return 1;
    }

    if (!ConnectToGame(username)) {
        _tprintf(_T("Failed to connect to the game.\n"));
        return 1;
    }

    _tprintf(_T("Bot '%s' connected with reaction time %d ms.\n"), username, reactionTime);

    HANDLE hListenerThread = CreateThread(NULL, 0, ListenToOverseer, NULL, 0, NULL);
    if (hListenerThread == NULL) {
        _tprintf(_T("Failed to create listener thread.\n"));
        CloseHandle(hPipe);
        return 1;
    }

    TCHAR botDictionary[10][MAX_WORD_LENGTH] = {
        _T("casa"), _T("pato"), _T("mesa"), _T("fogo"), _T("nuvem"),
        _T("vento"), _T("pedra"), _T("chao"), _T("livro"), _T("porta")
    };

    int dictionarySize = 10;
    srand((unsigned int)time(NULL));

    while (botRunning) {
        int randomIndex = rand() % dictionarySize;
        TCHAR* chosenWord = botDictionary[randomIndex];

        GAME_MESSAGE guess;
        _tcscpy_s(guess.sender, MAX_NAME_LENGTH, username);
        _tcscpy_s(guess.content, 256, chosenWord);
        guess.msgType = MSG_GUESS;

        if (SendMessageToOverseer(guess)) {
            _tprintf(_T("[Bot '%s'] Attempted word: %s\n"), username, chosenWord);
        }
        else {
            _tprintf(_T("[Bot '%s'] Failed to send word: %s\n"), username, chosenWord);
        }

        Sleep(reactionTime);
    }

    _tprintf(_T("Bot '%s' is shutting down.\n"), username);
    WaitForSingleObject(hListenerThread, INFINITE);
    CloseHandle(hListenerThread);
    CloseHandle(hPipe);

    return 0;
}