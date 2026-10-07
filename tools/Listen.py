"""Validate the mod's UDP and shared memory feeds without motion middleware."""
import argparse
import ctypes as C
from ctypes import wintypes as W
import json
import math
from pathlib import Path
import select
import socket
import struct
import time

MOTION=['RollDegrees','PitchDegrees','YawDegrees','RollSpeed','PitchSpeed','YawSpeed','LocalAccelerationX','LocalAccelerationY','LocalAccelerationZ']
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--seconds',type=float,default=10);p.add_argument('--json-port',type=int,default=20777);p.add_argument('--simtools-port',type=int,default=4123);p.add_argument('--external-port',type=int);p.add_argument('--output',type=Path);a=p.parse_args()
    feeds={};records=[]
    ports=[(a.json_port,'json'),(a.simtools_port,'simtools')]
    if a.external_port:ports.append((a.external_port,'external'))
    for port,kind in ports:
        s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
        try:s.bind(('127.0.0.1',port))
        except OSError as e:raise RuntimeError(f'Port {port} is busy; close the receiver or change the mod configuration.') from e
        feeds[s]=kind
    start=time.monotonic();counts={kind:0 for _,kind in ports};active=paused=0;maxima={k:0 for k in MOTION};last=None;externalLast=None;lastCounter=0
    try:
        while time.monotonic()-start<a.seconds:
            readable,_,_=select.select(list(feeds),[],[],0.2)
            for s in readable:
                payload=s.recv(65535);kind=feeds[s];counts[kind]+=1
                if kind=='external':
                    assert len(payload)==188
                    magic,schema,major,minor=struct.unpack_from('<IIHH',payload)
                    assert (magic,schema,major,minor)==(0xE53FF221,0xEC368575,1,0)
                    counter=struct.unpack_from('<Q',payload,21)[0];assert counter>lastCounter;lastCounter=counter
                    running,pausedFlag=struct.unpack_from('<BB',payload,29)
                    motion=struct.unpack_from('<12f',payload,55)
                    assert all(math.isfinite(v) for v in motion)
                    if pausedFlag:assert all(v==0 for v in motion)
                    externalLast={'size':len(payload),'counter':counter,'running':running,'paused':pausedFlag,'motion':motion}
                    continue
                data=payload.decode('utf-8')
                if kind=='json':
                    f=json.loads(data);last=f;active+=bool(f['OutputActive']);paused+=bool(f['Paused'])
                    assert f['TelemetryVersion']==1
                    assert all(math.isfinite(v) for v in f.values() if isinstance(v,(int,float)))
                    if not f['OutputActive']:assert all(f[k]==0 for k in MOTION)
                    for k in MOTION:maxima[k]=max(maxima[k],abs(f[k]))
                    records.append({'elapsed':time.monotonic()-start,'frame':f})
                else:
                    assert data.startswith('S~Roll:') and data.endswith('~E')
                    values=data[2:-2].split(':');assert len(values)==18
                    assert all(math.isfinite(float(v)) for v in values[1::2])
        assert all(counts.values()),f'Missing feed: {counts}'
    finally:
        for s in feeds:s.close()
    # Independent sequence-lock copy of the public shared memory ABI.
    K=C.WinDLL('kernel32',use_last_error=True)
    K.OpenFileMappingW.argtypes=[W.DWORD,W.BOOL,W.LPCWSTR];K.OpenFileMappingW.restype=W.HANDLE
    K.MapViewOfFile.argtypes=[W.HANDLE,W.DWORD,W.DWORD,W.DWORD,C.c_size_t];K.MapViewOfFile.restype=C.c_void_p
    K.UnmapViewOfFile.argtypes=[C.c_void_p];K.CloseHandle.argtypes=[W.HANDLE]
    h=K.OpenFileMappingW(4,False,'Local\\GalacticRacerTelemetryV1');shared=False
    if h:
        view=K.MapViewOfFile(h,4,0,0,220)
        try:
            if not view:raise C.WinError(C.get_last_error())
            for _ in range(1000):
                before=C.c_uint32.from_address(view).value
                if before&1:continue
                data=C.string_at(view+4,216);after=C.c_uint32.from_address(view).value
                if before==after and not(after&1):break
            else:raise RuntimeError('Could not obtain a stable shared memory frame.')
            magic,version,header,size=struct.unpack_from('<IHHI',data)
            assert (magic,version,header,size)==(0x31545247,1,48,216)
            shared=True
        finally:
            if view:K.UnmapViewOfFile(view)
            K.CloseHandle(h)
    report={'seconds':time.monotonic()-start,'packets':counts,'activeFrames':active,'pausedFrames':paused,'sharedMemoryValid':shared,'motionMaxAbs':maxima,'externalLast':externalLast,'lastFrame':last}
    if a.output:
        a.output.parent.mkdir(parents=True,exist_ok=True)
        a.output.write_text(json.dumps(report,indent=2),encoding='utf-8')
        a.output.with_suffix('.jsonl').write_text(''.join(json.dumps(r)+'\n' for r in records),encoding='utf-8')
    print(json.dumps(report,indent=2))
if __name__=='__main__':main()
