"""Load this mod in an already injected Galactic Racer, for development only."""
import argparse
import ctypes as C
from ctypes import wintypes as W
import json
import os
from pathlib import Path

K=C.WinDLL('kernel32',use_last_error=True)
P=C.c_void_p; S=C.c_size_t
K.OpenProcess.argtypes=[W.DWORD,W.BOOL,W.DWORD];K.OpenProcess.restype=W.HANDLE
K.CloseHandle.argtypes=[W.HANDLE]
K.VirtualAllocEx.argtypes=[W.HANDLE,P,S,W.DWORD,W.DWORD];K.VirtualAllocEx.restype=P
K.VirtualFreeEx.argtypes=[W.HANDLE,P,S,W.DWORD]
K.WriteProcessMemory.argtypes=[W.HANDLE,P,P,S,C.POINTER(S)]
K.GetModuleHandleW.argtypes=[W.LPCWSTR];K.GetModuleHandleW.restype=W.HMODULE
K.GetProcAddress.argtypes=[W.HMODULE,C.c_char_p];K.GetProcAddress.restype=P
K.LoadLibraryExW.argtypes=[W.LPCWSTR,W.HANDLE,W.DWORD];K.LoadLibraryExW.restype=W.HMODULE
K.FreeLibrary.argtypes=[W.HMODULE]
K.CreateRemoteThread.argtypes=[W.HANDLE,P,S,P,P,W.DWORD,C.POINTER(W.DWORD)];K.CreateRemoteThread.restype=W.HANDLE
K.WaitForSingleObject.argtypes=[W.HANDLE,W.DWORD]
K.GetExitCodeThread.argtypes=[W.HANDLE,C.POINTER(W.DWORD)]
K.CreateToolhelp32Snapshot.argtypes=[W.DWORD,W.DWORD];K.CreateToolhelp32Snapshot.restype=W.HANDLE
class Module(C.Structure):
    _fields_=[('dwSize',W.DWORD),('moduleId',W.DWORD),('pid',W.DWORD),('globalUsage',W.DWORD),('processUsage',W.DWORD),('base',P),('size',W.DWORD),('handle',W.HMODULE),('name',W.WCHAR*256),('path',W.WCHAR*260)]
K.Module32FirstW.argtypes=[W.HANDLE,C.POINTER(Module)]
K.Module32NextW.argtypes=[W.HANDLE,C.POINTER(Module)]
def modules(pid):
    h=K.CreateToolhelp32Snapshot(0x18,pid)
    if h==P(-1).value:raise C.WinError(C.get_last_error())
    try:
        e=Module();e.dwSize=C.sizeof(e);out=[];ok=K.Module32FirstW(h,C.byref(e))
        while ok:
            out.append(dict(name=e.name,path=e.path,base=e.base,size=e.size));ok=K.Module32NextW(h,C.byref(e))
        return out
    finally:K.CloseHandle(h)
def invoke(h,address,param=0):
    tid=W.DWORD();t=K.CreateRemoteThread(h,None,0,P(address),P(param),0,C.byref(tid))
    if not t:raise C.WinError(C.get_last_error())
    try:
        if K.WaitForSingleObject(t,30000)!=0:raise TimeoutError('Remote operation has not finished; do not unload the DLL.')
        code=W.DWORD()
        if not K.GetExitCodeThread(t,C.byref(code)):raise C.WinError(C.get_last_error())
        return code.value
    finally:K.CloseHandle(t)
def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('pid',type=int);parser.add_argument('dll',type=Path)
    parser.add_argument('--stop',action='store_true');parser.add_argument('--allow-isolated-test',action='store_true');args=parser.parse_args()
    rows=modules(args.pid)
    if not any(r['name'].lower()=='swgr-win64-shipping.exe' for r in rows):raise RuntimeError('Target is not Galactic Racer.')
    if not any(r['name'].lower()=='uevrbackend.dll' for r in rows):raise RuntimeError('Inject UEVR first.')
    path=args.dll.resolve(strict=True)
    if not args.stop and any(Path(r['path'])==path for r in rows):raise RuntimeError('This DLL is already loaded.')
    if not args.stop and any(r['name'].lower()=='galacticracermotion.dll' for r in rows):
        if not args.allow_isolated_test:raise RuntimeError('The plugin is already loaded. Restart before loading a new build.')
        config=Path(os.environ['APPDATA'])/'UnrealVRMod'/'SWGR-Win64-Shipping'/'galactic_racer_motion.cfg'
        values=dict(line.split('#')[0].replace(' ','').split('=',1) for line in config.read_text().splitlines() if '=' in line.split('#')[0])
        if values.get('SharedMemoryEnable')!='0' or values.get('SimHubPort')=='20777' or values.get('SimToolsPort')=='4123':raise RuntimeError('Isolated tests require shared memory disabled and non-default output ports.')
    h=K.OpenProcess(0x43A,False,args.pid)
    if not h:raise C.WinError(C.get_last_error())
    remote=None;finished=False
    try:
        if not args.stop:
            addr=K.GetProcAddress(K.GetModuleHandleW('kernel32.dll'),b'LoadLibraryW')
            owner=next(r for r in modules(os.getpid()) if r['base']<=addr<r['base']+r['size'])
            target=next(r for r in rows if r['name'].lower()==owner['name'].lower())
            load=target['base']+addr-owner['base']
            buffer=C.create_unicode_buffer(str(path));remote=K.VirtualAllocEx(h,None,C.sizeof(buffer),0x3000,4)
            if not remote:raise C.WinError(C.get_last_error())
            count=S()
            if not K.WriteProcessMemory(h,P(remote),buffer,C.sizeof(buffer),C.byref(count)) or count.value!=C.sizeof(buffer):raise C.WinError(C.get_last_error())
            invoke(h,load,remote);finished=True
        mod=next((r for r in modules(args.pid) if Path(r['path'])==path),None)
        if not mod:raise RuntimeError('LoadLibrary failed.')
        # Map only for export lookup: do not initialize SDK state in this helper.
        local=K.LoadLibraryExW(str(path),None,1)
        if not local:raise C.WinError(C.get_last_error())
        try:
            entry=K.GetProcAddress(local,b'SWGR_StopLive' if args.stop else b'SWGR_InitializeLive')
            if not entry:raise RuntimeError('Not a Galactic Racer Motion plugin.')
            result=invoke(h,mod['base']+entry-local)
        finally:K.FreeLibrary(local)
        if result:raise RuntimeError(f'Plugin initialization returned {result}.')
        print(json.dumps(dict(pid=args.pid,dll=str(path),initialized=not args.stop,stopped=args.stop)))
    finally:
        if remote and finished:K.VirtualFreeEx(h,P(remote),0,0x8000)
        K.CloseHandle(h)
if __name__=='__main__':main()
