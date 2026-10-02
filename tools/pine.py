# Minimal PINE client: PCSX2's remote control socket (read/write EE memory, status)
import socket, struct
OPS={'read8':0,'read16':1,'read32':2,'read64':3,'write8':4,'write16':5,'write32':6,'write64':7,'version':8,'save':9,'load':10,'title':11,'id':12,'uuid':13,'gameversion':14,'status':15}
class Pine:
    def __init__(self, path):
        self.s=socket.socket(socket.AF_UNIX,socket.SOCK_STREAM); self.s.settimeout(5); self.s.connect(path)
    def _call(self, payload):
        self.s.sendall(struct.pack('<I',len(payload)+4)+payload)
        head=self._recv(4); size=struct.unpack('<I',head)[0]
        body=self._recv(size-4)
        if body[0]!=0: raise RuntimeError('PINE failed')
        return body[1:]
    def _recv(self,n):
        b=b''
        while len(b)<n:
            c=self.s.recv(n-len(b))
            if not c: raise EOFError
            b+=c
        return b
    def read32(self,a): return struct.unpack('<I',self._call(struct.pack('<BI',2,a)))[0]
    def read8(self,a): return self._call(struct.pack('<BI',0,a))[0]
    def read64(self,a): return struct.unpack('<Q',self._call(struct.pack('<BI',3,a)))[0]
    def read_block(self,a,size,batch=16384):
        # Batches of read64 commands in one message each (PCSX2 answers them in order after one result byte)
        out=bytearray()
        for start in range(a,a+size,8*batch):
            count=min(batch,(a+size-start+7)//8)
            out+=self._call(b''.join(struct.pack('<BI',3,start+8*i) for i in range(count)))[:8*count]
        return bytes(out[:size])
    def write32(self,a,v): self._call(struct.pack('<BII',6,a,v))
    def write8(self,a,v): self._call(struct.pack('<BIB',4,a,v))
    def save_state(self, slot): self._call(struct.pack('<BB', 9, slot))
    def status(self): return struct.unpack('<I',self._call(struct.pack('<B',15)))[0]
    def title(self):
        r=self._call(struct.pack('<B',11)); n=struct.unpack('<I',r[:4])[0]; return r[4:4+n].rstrip(b'\0').decode('latin-1')
    def cstr(self,a,limit=128):
        out=b''
        while len(out)<limit:
            w=struct.pack('<I',self.read32(a+len(out)))
            for ch in w:
                if ch==0: return out.decode('latin-1')
                out+=bytes([ch])
        return out.decode('latin-1')
    def string(self,a):
        ptr=self.read32(a); n=self.read32(a+4)
        return self.cstr(ptr,n+1)[:n] if ptr else ''
