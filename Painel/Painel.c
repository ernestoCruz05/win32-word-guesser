#include <windows.h>
#include <tchar.h>
#include "../Project1/utils.h"
#include "resource1.h"


GameControlData cdata;
HWND g_hWnd;
HBITMAP hLogoBitmap = NULL;
RECT letrasRect;
HANDLE hPipe;
HWND hInput = NULL;

TCHAR g_displayedLetters[MAX_MAXLETTERS] = { 0 };
TCHAR name[32];
TCHAR szProgName[] = TEXT("Base");
TCHAR g_username[256];
TCHAR chatHistory[MAX_CHAT_LINES][256];
int currentLine = 0;

int panelWidth = 300;
int panelHeight = 80;
int panelY = 20;
int MAXLETRAS = 10;

void addLine(TCHAR* message)
{
    if (_tcslen(message) == 0) return;

    if (currentLine >= MAX_CHAT_LINES)
    {
	    for (int i = 1; i < MAX_CHAT_LINES; i++)
	    {
            _tcscpy_s(chatHistory[i - 1], 256, chatHistory[i]);
	    }
        currentLine = MAX_CHAT_LINES - 1;
    }
    _tcscpy_s(chatHistory[currentLine++], 256, message);
}

INT_PTR CALLBACK UsernameDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK) {
            GetDlgItemText(hDlg, IDC_USERNAME_INPUT, g_username, 256);
        	EndDialog(hDlg, IDOK);
            return (INT_PTR)TRUE;
        }
        else if (LOWORD(wParam) == IDCANCEL) {
            EndDialog(hDlg, IDCANCEL);
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}


// ============================================================================
// Programa base (esqueleto) para aplicações Windows    
// ============================================================================
// Cria uma janela de nome "Janela Principal" e pinta fundo de branco
// Modelo para programas Windows (composto por 2 funções): 
//    _tWinMain()    - Ponto de entrada dos programas windows
//                     1) Define, regista, cria e mostra a janela
//                     2) Loop de recepção de mensagens provenientes do Windows
//    trataEventos() - Processamentos da janela (pode ter outro nome)
//                     1) É chamada pelo Windows (callback) 
//                     2) Executa código em função da mensagem recebida

LRESULT CALLBACK trataEventos(HWND, UINT, WPARAM, LPARAM);

// Nome da classe da janela (para programas de uma só janela, normalmente este nome é 
// igual ao do próprio programa) "szprogName" é usado mais abaixo na definição das 
// propriedades do objecto janela


DWORD WINAPI LetterUpdateThread(GameControlData* cdata)
{

	TCHAR ll[MAX_MAXLETTERS] = { 0 };
    HANDLE hUpdateEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, TEXT("Global\\SharedMemUpdatedEvent"));

    WaitForSingleObject(cdata->hGameMutex, INFINITE);
    memcpy(ll, cdata->sharedMem->displayedLetters, MAXLETRAS);
    ReleaseMutex(cdata->hGameMutex);
	memcpy(g_displayedLetters, ll, MAXLETRAS);

    while (1) {
    WaitForSingleObject(hUpdateEvent, INFINITE);

	 WaitForSingleObject(cdata->hGameMutex, INFINITE);
        if (memcmp(ll, cdata->sharedMem->displayedLetters, MAXLETRAS)) {
            memcpy(ll, cdata->sharedMem->displayedLetters, MAXLETRAS);
            memcpy(g_displayedLetters, ll, MAXLETRAS);
			InvalidateRect(g_hWnd, NULL, TRUE);
        }
        ReleaseMutex(cdata->hGameMutex);
    }
    return 0;
}

DWORD WINAPI BroadcastUpdateThread(GameControlData* cdata)
{
    static TCHAR lastSeen[256] = _T("");
    HANDLE hUpdateEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, TEXT("Global\\Global\\BroadcastEvent"));

    while (1)
    {
        WaitForSingleObject(hUpdateEvent, INFINITE);

        WaitForSingleObject(cdata->hGameMutex, INFINITE);

        if (_tcscmp(lastSeen, cdata->sharedMem->uiBroadDisplay) != 0)
        {
            _tcscpy_s(lastSeen, 256, cdata->sharedMem->uiBroadDisplay);
            addLine(lastSeen);
            InvalidateRect(g_hWnd, NULL, TRUE); 
        }

        ReleaseMutex(cdata->hGameMutex);
    }

    return 0;
}

// ============================================================================
// FUNÇÃO DE INÍCIO DO PROGRAMA - _tWinMain()
// ============================================================================
// Em Windows, o programa começa sempre a sua execução na função _tWinMain()que desempenha
// o papel da função _tmain() do C em modo consola. WINAPI indica o "tipo da função" 
// (WINAPI para todas as declaradas nos headers do Windows e CALLBACK para as funções de
// processamento da janela)
// Parâmetros:
//    hInst     - Gerado pelo Windows, é o handle (número) da instância deste programa 
//    hPrevInst - Gerado pelo Windows, é sempre NULL para o NT (era usado no Windows 3.1)
//    lpCmdLine - Gerado pelo Windows, é um ponteiro para uma string terminada por 0
//                destinada a conter parâmetros para o programa 
//    nCmdShow  - Parâmetro que especifica o modo de exibição da janela (usado em  
//        	      ShowWindow()

int WINAPI _tWinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPTSTR lpCmdLine, int nCmdShow) {

    cdata.hMapFile = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, SHM_NAME);
    if (cdata.hMapFile == NULL) {
        MessageBox(NULL, _T("Failed to open shared memory"), _T("Error"), MB_OK);
        return 1;
    }

    cdata.sharedMem = (GameSharedMem*)MapViewOfFile(cdata.hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(GameSharedMem));
    if (cdata.sharedMem == NULL) {
        CloseHandle(cdata.hMapFile);
        MessageBox(NULL, _T("Failed to map shared memory"), _T("Error"), MB_OK);
        return 1;
    }

    // Open mutex
    cdata.hGameMutex = OpenMutex(MUTEX_ALL_ACCESS, FALSE, LETTER_MUTEX_NAME);
    if (cdata.hGameMutex == NULL) {
        MessageBox(NULL, _T("Failed to open mutex"), _T("Error"), MB_OK);
        return 1;
    }

    BOOL loaded = AddFontResourceEx(_T("Px437_Acer_VGA_8x8.ttf"), FR_PRIVATE, 0);
    if (!loaded) {
        MessageBox(NULL, _T("Font not loaded!"), _T("Error"), MB_OK | MB_ICONERROR);
    }

 

    HANDLE hThread = CreateThread(NULL, 0, LetterUpdateThread, &cdata, 0, NULL);
    HANDLE hBroadcastThread = CreateThread(NULL, 0, BroadcastUpdateThread, &cdata, 0, NULL);
        

    HWND hWnd;          // hWnd é o handler da janela, gerado mais abaixo por CreateWindow()
    MSG lpMsg;          // MSG é uma estrutura definida no Windows para as mensagens
    WNDCLASSEX wcApp;   // WNDCLASSEX é uma estrutura cujos membros servem para 

    wcApp.cbSize = sizeof(WNDCLASSEX);  // Tamanho da estrutura WNDCLASSEX
    wcApp.hInstance = hInst;  // Instância da janela actualmente exibida 
    // ("hInst" é parâmetro de _tWinMain e vem 
    // inicializada daí)
    wcApp.lpszClassName = szProgName;  // Nome da janela (neste caso = nome do programa)
    wcApp.lpfnWndProc = trataEventos;  // Endereço da função de processamento da janela
    // ("TrataEventos" foi declarada no início e
    // encontra-se mais abaixo)
    wcApp.style = CS_HREDRAW | CS_VREDRAW;  // Estilo da janela: Fazer o redraw se for
    // modificada horizontal ou verticalmente
    wcApp.hIcon = LoadIcon(NULL, IDI_APPLICATION);  // "hIcon" - handler do ícone normal
    // "NULL" - Ícone definido no Windows
    // "IDI_AP..." -  Ícone "aplicação"
    wcApp.hIconSm = LoadIcon(NULL, IDI_INFORMATION);  // "hIconSm" - handler do ícon pequeno
    // "NULL" - Ícone definido no Windows
    // "IDI_INF..." - Ícone de informação
    wcApp.hCursor = LoadCursor(NULL, IDC_ARROW);  // "hCursor" - handler do cursor (rato) 
    // "NULL" - Forma definida no Windows
    // "IDC_ARROW" - Aspecto "seta" 
    wcApp.lpszMenuName = NULL;  // Classe do menu que a janela pode ter
    // (NULL - não tem menu)
    wcApp.cbClsExtra = 0;  // Livre, para uso particular
    wcApp.cbWndExtra = 0;  // Livre, para uso particular
    wcApp.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);  // "hbrBackground" - handler
    //  para "brush" de pintura do fundo da janela. Devolvido por
    // "GetStockObject". Neste caso o fundo será branco.



    if (!RegisterClassEx(&wcApp))
        return(0);


    hWnd = CreateWindow(
        szProgName,  // Nome da janela (programa) definido acima
        TEXT("Painel"),  // Texto que figura na barra do título
        WS_OVERLAPPEDWINDOW,	// Estilo da janela (WS_OVERLAPPED - normal)
        CW_USEDEFAULT,  // Posição x pixels (default - à direita da última)
        CW_USEDEFAULT,  // Posição y pixels (default - abaixo da última)
        CW_USEDEFAULT,  // Largura da janela (em pixels)
        CW_USEDEFAULT,  // Altura da janela (em pixels)
        (HWND)HWND_DESKTOP,	// handle da janela pai (se se criar uma a partir de outra)
        //  ou HWND_DESKTOP se a janela for a primeira, criada a partir do "desktop"
        (HMENU)NULL,  // handle do menu da janela (se tiver menu)
        (HINSTANCE)hInst,  // handle da instância do programa actual ("hInst" é 
        // passado num dos parâmetros de _tWinMain()
        0);  // Não há parâmetros adicionais para a janela

    g_hWnd = hWnd;




    HFONT hFont = CreateFont(
        16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        _T("Px437 Acer VGA 8x8")
    );
    SendMessage(hInput, WM_SETFONT, (WPARAM)hFont, TRUE);





    hLogoBitmap = (HBITMAP)LoadImage(
        NULL,
        _T("teste2.bmp"), 
        IMAGE_BITMAP,
        0, 0,
        LR_LOADFROMFILE | LR_CREATEDIBSECTION
    );

    if (!hLogoBitmap) {
        MessageBox(NULL, _T("Could not load adivinha.bmp"), _T("Error"), MB_OK | MB_ICONERROR);
    }


    // ============================================================================
    // 4. Mostrar a janela
    // ============================================================================
    ShowWindow(hWnd, nCmdShow);  // "hWnd" - handler da janela, devolvido por 
    // "CreateWindow"; "nCmdShow" - modo de exibição (p.e. 
    // normal/modal); é passado como parâmetro de _tWinMain()
    UpdateWindow(hWnd);  // Refrescar a janela (Windows envia à janela uma 
    // mensagem para pintar, mostrar dados, (refrescar)… 


    while (GetMessage(&lpMsg, NULL, 0, 0) > 0) {
        TranslateMessage(&lpMsg);  // Pré-processamento da mensagem (p.e. obter código 
        // ASCII da tecla premida)
        DispatchMessage(&lpMsg);  // Enviar a mensagem traduzida de volta ao Windows, que
        // aguarda até que a possa reenviar à função de 
        // tratamento da janela, CALLBACK trataEventos (abaixo)
    }
    return (int)lpMsg.wParam;  
}


LRESULT CALLBACK trataEventos(HWND hWnd, UINT messg, WPARAM wParam, LPARAM lParam) {
    switch (messg) {
    case WM_DESTROY:	     // Destruir a janela e terminar o programa 
        PostQuitMessage(0);  // "PostQuitMessage(Exit Status)"

        if (hLogoBitmap) {
            DeleteObject(hLogoBitmap);
        }

        break;
    case WM_PAINT:
    {

        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        RECT client;
        GetClientRect(hWnd, &client);

        HBRUSH bgBrush = CreateSolidBrush(RGB(20, 20, 60)); 
        FillRect(hdc, &client, bgBrush);
        DeleteObject(bgBrush);

        int w = client.right;
        int h = client.bottom;
        int padding = 20;

        HFONT hFont = CreateFont(
            18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE,
            _T("Px437 Acer VGA 8x8")
        );

    		if (hLogoBitmap) {
            HDC hdcMem = CreateCompatibleDC(hdc);
            HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hLogoBitmap);

            BITMAP bmp;
            GetObject(hLogoBitmap, sizeof(BITMAP), &bmp);


            int scaledWidth = bmp.bmWidth / 2;  
            int scaledHeight = bmp.bmHeight / 2;

            int x = (client.right - client.left - bmp.bmWidth) / 2 + bmp.bmWidth / 3.8;
            int y = 20; 

            TransparentBlt(
                hdc, x, y, scaledWidth, scaledHeight,  
                hdcMem, 0, 0, bmp.bmWidth, bmp.bmHeight,  
                RGB(255, 0, 255)  
            );

            SelectObject(hdcMem, hOldBitmap);
            DeleteDC(hdcMem);
        }

        int topGap = 60;              
        int fullWidth = client.right - client.left;
        int fullHeight = client.bottom - client.top;

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(0, 0, 0));


        HFONT oldFont = (HFONT)SelectObject(hdc, hFont);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(0, 255, 255));

        HBRUSH blackBrush = CreateSolidBrush(RGB(0, 0, 0));
        HPEN cyanPen = CreatePen(PS_SOLID, 1, RGB(0, 150, 255));

        HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, blackBrush);
        HPEN oldPen = (HPEN)SelectObject(hdc, cyanPen);

        TCHAR letters[64] = _T("");
        WaitForSingleObject(cdata.hGameMutex, INFINITE);
        for (int i = 0; i < MAXLETRAS; i++) {
            _stprintf_s(letters + _tcslen(letters), 64 - _tcslen(letters), _T("%c  "), cdata.sharedMem->displayedLetters[i]);
        }
        ReleaseMutex(cdata.hGameMutex);

        // Top Letter Display
        RECT test = { 1 , 1 ,1 ,1 };
        letrasRect.left = padding;
    		letrasRect.top = padding + 230 + 10;
            letrasRect.right = w - padding;
            letrasRect.bottom = padding + 310;
     
        Rectangle(hdc, letrasRect.left, letrasRect.top, letrasRect.right, letrasRect.bottom);
        DrawText(hdc, letters, -1, &letrasRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);


        // Stats Left
        int statBoxWidth = (w - 3 * padding) / 2;
        RECT statsLeft = { padding, letrasRect.bottom + padding, padding + statBoxWidth, letrasRect.bottom + padding + 350 };
        Rectangle(hdc, statsLeft.left, statsLeft.top, statsLeft.right, statsLeft.bottom);

		int yOffset = statsLeft.top + 25;  
        int lineHeight = 20;

        WaitForSingleObject(cdata.hGameMutex, INFINITE);

        for (int i = 0; i < 10; ++i) {
            if (_tcslen(cdata.sharedMem->uiLeaderboardNames[i]) == 0)
                break;

            TCHAR line[128];
            _stprintf_s(line, 128, _T("%d. %s - %d pts"), i + 1,
                cdata.sharedMem->uiLeaderboardNames[i],
                cdata.sharedMem->uiLeaderboardPoints[i]);

            RECT lineRect = {
                statsLeft.left + 10,
                yOffset,
                statsLeft.right - 10,
                yOffset + lineHeight
            };

            DrawText(hdc, line, -1, &lineRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            yOffset += lineHeight;
        }

        ReleaseMutex(cdata.hGameMutex);

        // Chat
        RECT statsRight = { statsLeft.right + padding, statsLeft.top, statsLeft.right + padding + statBoxWidth, statsLeft.bottom };
        Rectangle(hdc, statsRight.left, statsRight.top, statsRight.right, statsRight.bottom);

        int chatYOffset = statsRight.top + 5;
        int maxChatHeight = statsRight.bottom - statsRight.top - 5;

        for (int i = max(0, currentLine - MAX_CHAT_LINES); i < currentLine; i++) {
            RECT lineRect = {
                statsRight.left + 10,
                chatYOffset,
                statsRight.right - 10,
                statsRight.bottom
            };

            
            RECT tempRect = lineRect;
            DrawText(hdc, chatHistory[i], -1, &tempRect, DT_LEFT | DT_WORDBREAK | DT_CALCRECT);

            int lineHeight = tempRect.bottom - tempRect.top;

            if (chatYOffset + lineHeight > statsRight.bottom)
                break;

            DrawText(hdc, chatHistory[i], -1, &lineRect, DT_LEFT | DT_WORDBREAK);
            chatYOffset += lineHeight;
        }



        // Last guess
        TCHAR lastGuess[256] = _T("");
        WaitForSingleObject(cdata.hGameMutex, INFINITE);

        if (cdata.sharedMem->uiLastGuess[0] != '\0') {
            _stprintf_s(lastGuess,  _countof(lastGuess), _T("Last correct guess: %s"), cdata.sharedMem->uiLastGuess);
        }
        else {
            _tcscpy_s(lastGuess, _countof(lastGuess), _T("Last correct guess: [none]"));
        }

        ReleaseMutex(cdata.hGameMutex);

        RECT chatRect = { padding, statsLeft.bottom + padding, w - padding, statsLeft.bottom + padding + 90 };
        Rectangle(hdc, chatRect.left, chatRect.top, chatRect.right, chatRect.bottom);
        DrawText(hdc, lastGuess, -1, &chatRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        // Cleanup
        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldFont);

        DeleteObject(blackBrush);
        DeleteObject(hFont);
        DeleteObject(cyanPen);

        EndPaint(hWnd, &ps);
    }
    break;
    case WM_CTLCOLOREDIT:
	    {
        HDC hdcEdit = (HDC)wParam;

        SetTextColor(hdcEdit, RGB(0, 255, 255));
        SetBkColor(hdcEdit, RGB(0, 0, 0));

        static HBRUSH hBrush = NULL;
        if (!hBrush)
            hBrush = CreateSolidBrush(RGB(0, 0, 0));  

        return (INT_PTR)hBrush;
	    }
        break;
    default:
        // Neste exemplo, para qualquer outra mensagem (p.e. "minimizar","maximizar","restaurar")
        // não é efectuado nenhum processamento, apenas se segue o "default" do Windows
        return DefWindowProc(hWnd, messg, wParam, lParam);
    }
    return 0;
}
