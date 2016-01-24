/*++

Copyright (c) 1999 by Antoni Sawicki

Module Name:

    regln.c

Abstract:

    Manage Windows Registry Symbolic Links

Author:

    Antoni Sawicki <as@tenoware.com>

Credits:
    
    Antoni Sawicki <as@tenoware.com>
    Tomasz Nowak <tn@tenoware.com>
    Mark Russinovitch <mark@sysinternals.com>

License:
   
    BSD

--*/

#include <windows.h>
#include <wchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#define REGLN_LINK_VALUE_NAME         L"SymbolicLinkValue" 
#define REGLN_OPTION_CREATE_LINK      (0x00000002L)
#define REGLN_OPTION_OPEN_LINK        (0x00000100L)
#define REGLN_OBJ_CASE_INSENSITIVE    (0x00000040L)

typedef struct _PROG_ARGS {
    ULONG Volatile;
    ULONG Delete;
    WCHAR SourceKey[1024];
    WCHAR DestinationKey[1024];
} PROG_ARGS;
typedef PROG_ARGS *PPROG_ARGS;

typedef struct _UNICODE_STRING {
    WORD Length;
    WORD MaximumLength;
    PWSTR  Buffer;
} UNICODE_STRING;
typedef UNICODE_STRING *PUNICODE_STRING;

typedef struct _OBJECT_ATTRIBUTES {
    ULONG Length;
    HANDLE RootDirectory;
    PUNICODE_STRING ObjectName;
    ULONG Attributes;
    PVOID SecurityDescriptor; 
    PVOID SecurityQualityOfService;
} OBJECT_ATTRIBUTES;
typedef OBJECT_ATTRIBUTES *POBJECT_ATTRIBUTES;

NTSTATUS (__stdcall *NtCreateKey)(
    PHANDLE KeyHandle, 
    ULONG DesiredAccess, 
    POBJECT_ATTRIBUTES ObjectAttributes,
    ULONG TitleIndex, 
    PUNICODE_STRING Class, 
    ULONG CreateOptions, 
    PULONG Disposition 
);

NTSTATUS (__stdcall *NtSetValueKey)(
    HANDLE  KeyHandle,
    PUNICODE_STRING  ValueName,
    ULONG  TitleIndex,
    ULONG  Type,
    PVOID  Data,
    ULONG  DataSize
);

NTSTATUS (__stdcall *NtDeleteKey)(
    HANDLE KeyHandle
);

NTSTATUS (__stdcall *NtClose)(
    HANDLE Handle
);


VOID
Usage(
    void
    )
/*++

Routine Description:

    Display usage information and version info
    Terminate programm 

--*/
{
    wprintf(L"REGLN v2.2, Copyright (c) 1999 by Antoni Sawicki\n\n");
    wprintf(L"Usage: regln [-v] <link_key>  <target_key>\n");
    wprintf(L"       regln  -d  <link_key>\n\n");
    wprintf(L"Where: <link_key> is the new registry link key\n");
    wprintf(L"       <target_key> is an existing registry key being linked to\n");
    wprintf(L"       -v = volatile, exist in memory only\n");
    wprintf(L"       -d = delete link\n\n");
    wprintf(L"Examples:\n");
    wprintf(L"       regln HKLM\\SOFTWARE\\BigCompany HKLM\\SOFTWARE\\Microsoft\n");
    wprintf(L"       regln -d \\Registry\\Machine\\SOFTWARE\\BigCompany\n\n");
    ExitProcess(0);
}



VOID
DEBUG(
    WCHAR *msg,
    ...
    )
/*++

Routine Description:

    Print debug message if debug is enabled

--*/
{
    va_list valist;
    WCHAR vaBuff[1024]={0};

    if(_wgetenv(L"REGLN_DEBUG")) {
        va_start(valist, msg);
        _vsnwprintf(vaBuff, sizeof(vaBuff), msg, valist);
        va_end(valist);

        wprintf(L"DEBUG: %s\n", vaBuff);
    }
}


VOID
ERRPT(
    int EmitUsage,
    WCHAR *msg,
    ...
    )
/*++

Routine Description:

    Display User Error Information (eg. wrong parameter)
    Display actual error message
    Display usage information if flag specified
    Terminate program

--*/
{
    va_list valist;
    WCHAR vaBuff[1024]={0};
    WCHAR errBuff[1024]={0};
    DWORD err;

    va_start(valist, msg);
    _vsnwprintf(vaBuff, sizeof(vaBuff), msg, valist);
    va_end(valist);

    wprintf(L"ERROR: %s\n", vaBuff);

    err=GetLastError();

    if(err) {
        FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), errBuff, sizeof(errBuff) , NULL );
        wprintf(L"%08X : %s\n\n", err, errBuff);
    }    
    
    wprintf(L"\n\n");

    if(EmitUsage)
        Usage();

    ExitProcess(1);
}




VOID
NTAPI_Init(
    void
    )
/*++

Routine Description:

    This routine finds entry points for a few NTAPI functions

--*/
{
    if(!(NtCreateKey = (void *) GetProcAddress(GetModuleHandle("ntdll.dll"), "NtCreateKey" ))) 
        ERRPT(0, L"Unable to initialize NTAPI\n");

    if(!(NtDeleteKey = (void *) GetProcAddress(GetModuleHandle("ntdll.dll"), "NtDeleteKey" ))) 
        ERRPT(0, L"Unable to initialize NTAPI\n");

    if(!(NtSetValueKey = (void *) GetProcAddress(GetModuleHandle("ntdll.dll"), "NtSetValueKey" ))) 
        ERRPT(0, L"Unable to initialize NTAPI\n");

    if(!(NtClose = (void *) GetProcAddress(GetModuleHandle("ntdll.dll"), "NtClose" ))) 
        ERRPT(0, L"Unable to initialize NTAPI\n");
}


VOID 
WinToNtPath(
    WCHAR *Src,
    size_t DstSize,
    WCHAR *Dst
    ) 
/*++

Routine Description:
    
    Convert Win32 Registry Root Key Name to NT Namespace Path

    HKEY_LOCAL_MACHINE\Key -> \Registry\Machine\Key

--*/
{

    if(wcsnicmp(Src, L"HKEY_LOCAL_MACHINE\\", 19)==0 || wcsnicmp(Src, L"HKLM\\", 5)==0) 
        _snwprintf(Dst, DstSize, L"\\Registry\\Machine%s", wcschr(Src, L'\\'));

    else if(wcsnicmp(Src, L"HKEY_USERS\\", 11)==0 || wcsnicmp(Src, L"HKUS\\", 5)==0 || wcsnicmp(Src, L"HKU\\", 4)==0) 
        _snwprintf(Dst, DstSize, L"\\Registry\\User%s", wcschr(Src, L'\\'));

    else 
        ERRPT(1, L"WinToNtPtah: Wrong Win32 Registry Root Key Name.");

    DEBUG(L"WinToNtPath converted \"%s\" [len=%d] to \"%s\" [len=%d] [max=%d]", Src, wcslen(Src), Dst, wcslen(Dst), DstSize);
}



VOID
ParseArgs(
    int argc,
    WCHAR **argv,
    PPROG_ARGS args
    )
/*++

Routine Description:

    Parse arguments specified to Regln main() module
    Output is returned through PROG_ARGS structure

--*/
{
    int argn;

    DEBUG(L"argc: %d", argc);

    // parse all args
    for(argn=1; argn<argc; argn++) {
        DEBUG(L"argv[%d]: %s", argn, argv[argn]); 

        // this is an option
        if(argv[argn][0]==L'/' || argv[argn][0]==L'-') {

            if(argv[argn][1]) {
                DEBUG(L"option, len=%d", wcslen(argv[argn])); 
                switch(argv[argn][1]) {
                    case L'v':
                    case L'V':
                                DEBUG(L"Volatile set");
                                args->Volatile=1;
                                break;
                    case L'd':
                    case L'D':
                                DEBUG(L"Delete set");
                                args->Delete=1;
                                break;
                    case L'h':
                    case L'H':
                                Usage();
                                break;
                    default:
                                ERRPT(1, L"Unknown option: %s", argv[argn]);
                                break;
                }
            }
            else Usage();

        }

        // this is a parameter
        else {
            DEBUG(L"parameter, len=%d", wcslen(argv[argn])); 

            if(wcslen(argv[argn])>5) {

                if(!wcslen(args->SourceKey)) {

                    if(wcsnicmp(L"HK", argv[argn], 2)==0) 
                        WinToNtPath(argv[argn], sizeof(args->SourceKey), args->SourceKey);
                    
                    else if(wcsnicmp(L"\\Registry\\", argv[argn], 10)==0)
                        wcsncpy(args->SourceKey, argv[argn], sizeof(args->SourceKey));

                    else
                        ERRPT(1, L"Unknown Registry Root Key Format.");
                    
                    DEBUG(L"SourceKey set to \"%s\" [len=%d] [max=%d]", args->SourceKey, wcslen(args->SourceKey), sizeof(args->SourceKey));

                }
                else if(!wcslen(args->DestinationKey)) {

                    if(wcsnicmp(L"HK", argv[argn], 2)==0) 
                        WinToNtPath(argv[argn], sizeof(args->DestinationKey), args->DestinationKey);
                    
                    else if(wcsnicmp(L"\\Registry\\", argv[argn], 10)==0)
                        wcsncpy(args->DestinationKey, argv[argn], sizeof(args->DestinationKey));
                    
                    else
                        ERRPT(1, L"Unknown Registry Root Key Format.");
                    
                    DEBUG(L"DestinationKey set to \"%s\" [len=%d] [max=%d]", args->DestinationKey, wcslen(args->DestinationKey), sizeof(args->DestinationKey));

                }
                else 
                    ERRPT(1, L"Too many parameters. Giving up.");
                
            }
            else 
                ERRPT(1, L"Parameter %d [%s] too short. Giving up.", argn, argv[argn]);
            
        }
    }


    // check validity
    if(args->Delete) {

        if(!wcslen(args->SourceKey) || wcslen(args->DestinationKey)) 
            ERRPT(1, L"Delete requires one parameter.");

        if(args->Volatile) 
            ERRPT(1, L"Options -d, -v cannot be specified together.");

    }
    else {

        if(!wcslen(args->SourceKey) || !wcslen(args->DestinationKey)) 
            ERRPT(1, L"Two parameters required.");

    }

    if(args->Delete) 
        DEBUG(L">> Will delete link key %s", args->SourceKey);
    else 
        DEBUG(L">> Will create %s link key %s -> %s", (args->Volatile) ? L"volatile" : L"permanent", args->SourceKey, args->DestinationKey);

}



VOID
RegLnDeleteLink(
    WCHAR *LinkKeyName
    )
/*++

Routine Description:

    Delete a registry link

Arguments:

    Registry Link Key Name as NT Namespace Path

--*/
{
    DWORD Disposition, Status; 
    HANDLE NtKeyHandle; 
    UNICODE_STRING NtKeyName; 
    OBJECT_ATTRIBUTES NtObjAttr; 

    ZeroMemory(&NtKeyName, sizeof(UNICODE_STRING));
    ZeroMemory(&NtObjAttr, sizeof(OBJECT_ATTRIBUTES));

    NtKeyName.Buffer = LinkKeyName; 
    NtKeyName.Length = wcslen(LinkKeyName) * sizeof(WCHAR);

    NtObjAttr.ObjectName = &NtKeyName;
    NtObjAttr.Attributes = REGLN_OBJ_CASE_INSENSITIVE | REGLN_OPTION_OPEN_LINK; 
    NtObjAttr.RootDirectory =            NULL;   
    NtObjAttr.SecurityDescriptor =       NULL;   
    NtObjAttr.SecurityQualityOfService = NULL;   
    NtObjAttr.Length = sizeof(OBJECT_ATTRIBUTES);

    Status=NtCreateKey(&NtKeyHandle, KEY_ALL_ACCESS, &NtObjAttr, 0,  NULL, REG_OPTION_NON_VOLATILE, &Disposition);

    if(Status) 
        ERRPT(0, L"Link deletion failed. [%08X]\n", Status);

    Status=NtDeleteKey(NtKeyHandle);

    if(Status) 
        ERRPT(0, L"Link deletion failed. [%08X]\n", Status);

    Status=NtClose(NtKeyHandle);

    if(Status)
        ERRPT(0, L"NtClose failed. [%08X]\n", Status);

    DEBUG(L"Link Delete Succeeded");

}


VOID
RegLnCreateLink(
    WCHAR *LinkKeyName,
    WCHAR *TargetKeyName,
    BOOL Volatile
    )
/*++

Routine Description:

    Create a registry link

Arguments:

    Registry Link Key Name as NT Namespace Path
    Registry Target Key Name as NT Namespace Path
    Volatile true/false

--*/
{
    ULONG Disposition;
    DWORD Status; 
    HANDLE NtKeyHandle; 
    UNICODE_STRING NtKeyName, NtValueName; 
    OBJECT_ATTRIBUTES NtObjAttr; 

    ZeroMemory(&NtKeyName, sizeof(UNICODE_STRING));
    ZeroMemory(&NtValueName, sizeof(UNICODE_STRING));
    ZeroMemory(&NtObjAttr, sizeof(OBJECT_ATTRIBUTES));

    NtKeyName.Buffer = LinkKeyName;
    NtKeyName.Length = wcslen(LinkKeyName) * sizeof(WCHAR);

    NtValueName.Buffer = REGLN_LINK_VALUE_NAME;
    NtValueName.Length = wcslen(REGLN_LINK_VALUE_NAME) * sizeof(WCHAR);

    NtObjAttr.ObjectName = &NtKeyName;
    NtObjAttr.Attributes = REGLN_OBJ_CASE_INSENSITIVE; 
    NtObjAttr.RootDirectory =            NULL;   
    NtObjAttr.SecurityDescriptor =       NULL;   
    NtObjAttr.SecurityQualityOfService = NULL;   
    NtObjAttr.Length = sizeof(OBJECT_ATTRIBUTES);

    
    Status=NtCreateKey(&NtKeyHandle, KEY_ALL_ACCESS, &NtObjAttr, 0,  NULL, (Volatile) ? REG_OPTION_VOLATILE|REGLN_OPTION_CREATE_LINK : REG_OPTION_NON_VOLATILE|REGLN_OPTION_CREATE_LINK, &Disposition);

    if(Status)
        ERRPT(0, L"Unable to create registry key \"%s\" [%08X]", LinkKeyName, Status);

    Status=NtSetValueKey(NtKeyHandle, &NtValueName, 0, REG_LINK, TargetKeyName, wcslen(TargetKeyName) * sizeof(WCHAR));

    if(Status)
        ERRPT(0, L"Unable to set link value for [REG_LINK]%s=\"%s\" [%08X]", REGLN_LINK_VALUE_NAME, TargetKeyName, Status);

    Status=NtClose(NtKeyHandle);

    if(Status)
        ERRPT(0, L"NtClose failed. [%08X]\n", Status);

    DEBUG(L"Link Creation Succeeded");

}



int
wmain(
    int argc, 
    WCHAR **argv
    )
/*++

Routine Description:

    This routine is the main program for Regln.

    The arguments accepted by Regln are:

        /d                  Delete registry link
        /v                  Volatile operation
        link key            Registry link being created or deleted
        target key          Registry key being linked to

--*/
{
    PROG_ARGS RegLnArgs;

    ZeroMemory(&RegLnArgs, sizeof(PROG_ARGS));
    
    NTAPI_Init(); 

    ParseArgs(argc, argv, &RegLnArgs); 
    
    if(RegLnArgs.Delete) 
        RegLnDeleteLink(RegLnArgs.SourceKey);
    else
        RegLnCreateLink(RegLnArgs.SourceKey, RegLnArgs.DestinationKey, RegLnArgs.Volatile);
    
    return 0;
};



