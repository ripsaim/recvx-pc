#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wininet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "version.h"
#include "release_json.h"

#define API_URL "https://api.github.com/repos/ripsaim/recvx-pc/releases/latest"
#define MAX_RESPONSE RECVX_JSON_MAX_BYTES
#define NOTES_CAP RECVX_JSON_BODY_CAP
typedef struct { unsigned long major, minor, patch; } Version;
static char installed[64], offered[64], release_notes[NOTES_CAP];
static int update_available, chose_install;

static int parse_version(const char *s, Version *v)
{
    unsigned long values[3]; int i;
    if (!s || !*s) return 0;
    for (i=0; i<3; ++i) {
        const char *start=s; unsigned long n=0;
        if (*s<'0'||*s>'9') return 0;
        if (*s=='0' && s[1]>='0' && s[1]<='9') return 0;
        while (*s>='0'&&*s<='9') {
            unsigned digit=(unsigned)(*s-'0');
            if (n>((unsigned long)0xffffffffUL-digit)/10UL) return 0;
            n=n*10UL+digit; ++s;
        }
        if (s==start) return 0;
        values[i]=n;
        if (i<2) { if (*s++!='.') return 0; }
        else if (*s) return 0;
    }
    v->major=values[0];v->minor=values[1];v->patch=values[2];return 1;
}

static int compare_version(Version a, Version b)
{
    if (a.major != b.major) return a.major > b.major ? 1 : -1;
    if (a.minor != b.minor) return a.minor > b.minor ? 1 : -1;
    if (a.patch != b.patch) return a.patch > b.patch ? 1 : -1;
    return 0;
}

static int check_release(void)
{
    HINTERNET h=NULL,r=NULL; char *data=NULL; RecvxRelease release; DWORD timeout=6000, status=0, status_len=sizeof(status), got, used=0;
    Version local, remote; int result=0;
    if(!parse_version(RECVX_VERSION_STR,&local))return 0;
    h=InternetOpenA("recvx-launcher/1.1",INTERNET_OPEN_TYPE_PRECONFIG,NULL,NULL,0); if(!h)goto done;
    InternetSetOptionA(h,INTERNET_OPTION_CONNECT_TIMEOUT,&timeout,sizeof(timeout));
    InternetSetOptionA(h,INTERNET_OPTION_RECEIVE_TIMEOUT,&timeout,sizeof(timeout));
    r=InternetOpenUrlA(h,API_URL,"Accept: application/vnd.github+json\r\nUser-Agent: recvx-launcher\r\n",-1,
        INTERNET_FLAG_SECURE|INTERNET_FLAG_RELOAD|INTERNET_FLAG_NO_CACHE_WRITE,0);
    if(!r||!HttpQueryInfoA(r,HTTP_QUERY_STATUS_CODE|HTTP_QUERY_FLAG_NUMBER,&status,&status_len,NULL)||status!=200)goto done;
    data=(char*)malloc(MAX_RESPONSE+1); if(!data)goto done;
    while(used<MAX_RESPONSE) {
        if(!InternetReadFile(r,data+used,MAX_RESPONSE-used,&got))goto done;
        if(!got)break;
        used+=got;
    }
    if(used==MAX_RESPONSE)goto done;
    if(!recvx_parse_release(data,used,&release))goto done;
    if(release.tag[0]=='v')memmove(release.tag,release.tag+1,strlen(release.tag));
    if(!parse_version(release.tag,&remote))goto done;
    result=1;
    if(compare_version(remote,local)>0){
        strcpy(offered,release.tag); strcpy(release_notes,release.body); update_available=1;
    }
done:
    if(data)free(data); if(r)InternetCloseHandle(r); if(h)InternetCloseHandle(h); return result;
}

static int run_game(void)
{
    char self[MAX_PATH], game[MAX_PATH]; char *slash; STARTUPINFOA si; PROCESS_INFORMATION pi; DWORD n;
    n=GetModuleFileNameA(NULL,self,MAX_PATH); if(!n||n>=MAX_PATH)return 0;
    slash=strrchr(self,'\\'); if(!slash)return 0; *slash=0;
    if(snprintf(game,sizeof(game),"%s\\recvx.exe",self)>=(int)sizeof(game))return 0;
    if(GetFileAttributesA(game)==INVALID_FILE_ATTRIBUTES){MessageBoxA(NULL,"recvx.exe was not found beside the launcher.","RECVX Launcher",MB_OK|MB_ICONERROR);return 0;}
    ZeroMemory(&si,sizeof(si)); si.cb=sizeof(si); ZeroMemory(&pi,sizeof(pi));
    if(!CreateProcessA(game,NULL,NULL,NULL,FALSE,0,NULL,self,&si,&pi)){MessageBoxA(NULL,"Could not start recvx.exe.","RECVX Launcher",MB_OK|MB_ICONERROR);return 0;}
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess); return 1;
}

static LRESULT CALLBACK prompt_proc(HWND w,UINT m,WPARAM wp,LPARAM lp)
{
    if(m==WM_COMMAND&&LOWORD(wp)==1){chose_install=1;DestroyWindow(w);return 0;}
    if(m==WM_COMMAND&&LOWORD(wp)==2){DestroyWindow(w);return 0;}
    if(m==WM_CLOSE){DestroyWindow(w);return 0;} return DefWindowProcA(w,m,wp,lp);
}

static void prompt(void)
{
    WNDCLASSA wc; HWND w,c; MSG msg; char versions[160]; HINSTANCE h=GetModuleHandleA(NULL);
    ZeroMemory(&wc,sizeof(wc));wc.lpfnWndProc=prompt_proc;wc.hInstance=h;wc.hCursor=LoadCursor(NULL,IDC_ARROW);wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);wc.lpszClassName="RecvxUpdatePrompt";
    RegisterClassA(&wc);
    w=CreateWindowExA(WS_EX_DLGMODALFRAME,wc.lpszClassName,"Resident Evil Code: Veronica X",WS_CAPTION|WS_SYSMENU|WS_VISIBLE,CW_USEDEFAULT,CW_USEDEFAULT,570,470,NULL,NULL,h,NULL);
    if(!w)return;
    c=CreateWindowA("STATIC","A new update has arrived!",WS_CHILD|WS_VISIBLE,22,18,520,32,w,NULL,h,NULL);SendMessageA(c,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),TRUE);
    sprintf(versions,"Current version: %s\r\nNew version: %s",installed,offered);
    c=CreateWindowA("STATIC",versions,WS_CHILD|WS_VISIBLE,22,56,520,48,w,NULL,h,NULL);SendMessageA(c,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),TRUE);
    c=CreateWindowExA(WS_EX_CLIENTEDGE,"EDIT",release_notes[0]?release_notes:"No release notes provided.",WS_CHILD|WS_VISIBLE|ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL|WS_VSCROLL,22,110,520,275,w,NULL,h,NULL);SendMessageA(c,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),TRUE);
    c=CreateWindowA("BUTTON","Install update",WS_CHILD|WS_VISIBLE|BS_DEFPUSHBUTTON,280,400,130,30,w,(HMENU)1,h,NULL);SendMessageA(c,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),TRUE);
    c=CreateWindowA("BUTTON","Later",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,420,400,120,30,w,(HMENU)2,h,NULL);SendMessageA(c,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),TRUE);
    while(IsWindow(w)&&GetMessageA(&msg,NULL,0,0)>0){TranslateMessage(&msg);DispatchMessageA(&msg);}
}

int WINAPI WinMain(HINSTANCE a,HINSTANCE b,LPSTR c,int d)
{
    (void)a;(void)b;(void)c;(void)d; strcpy(installed,RECVX_VERSION_STR);
    check_release(); /* no network, malformed response, rate limit: run installed game */
    if(update_available)prompt();
    if(chose_install)MessageBoxA(NULL,"Updating is not implemented yet. No files were changed. The current game will now start.","Update unavailable",MB_OK|MB_ICONINFORMATION);
    run_game(); return 0;
}
