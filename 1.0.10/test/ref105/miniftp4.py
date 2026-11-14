import posixpath, time, socket, threading, os, sys, stat, time, shutil
ROOT = os.path.abspath(sys.argv[1]); PORT = int(sys.argv[2]); LOG = open(sys.argv[3], 'a')
def real(cwd, p):
    p = p if p.startswith('/') else (cwd.rstrip('/') + '/' + p)
    p = posixpath.normpath(p)
    if p.startswith('//'): p = p[1:]
    return p, ROOT + p
FAKE = set(x.strip('/') for x in os.environ.get('FAKELINKS','').split(',') if x.strip())
def fake(full): return os.path.relpath(full, ROOT).replace(os.sep, '/') in FAKE
def ls_line(full, name):
    st = os.lstat(full); m = stat.filemode(st.st_mode)
    if fake(full): m = 'l' + m[1:]
    ziel = (' -> ' + os.readlink(full)) if (stat.S_ISLNK(st.st_mode) and os.environ.get('LINKZIEL')) else ''
    return f"{m} 1 1 0 {st.st_size} {time.strftime('%b %d %H:%M', time.localtime(st.st_mtime))} {name}{ziel}"
def handle(c):
    f = c.makefile('rwb', buffering=0)
    def send(s): f.write((s + '\r\n').encode())
    # BANNER: eigene Begrüßung, Zeilen mit | getrennt (z.B. wie ftpsrv); sonst '220 mini'
    for z in (os.environ.get('BANNER') or '220 mini').split('|'): send(z)
    cwd = '/'; pasv = None; rnfr = None
    while True:
        line = f.readline()
        if not line: break
        line = line.decode('utf-8', 'replace').rstrip('\r\n'); LOG.write(line + '\n'); LOG.flush()
        cmd, _, arg = line.partition(' '); cmd = cmd.upper()
        if os.environ.get('DIEON') and os.environ['DIEON'] in line:
            try: c.shutdown(socket.SHUT_RDWR)
            except Exception: pass
            c.close(); return
        try:
            if cmd == 'USER': send('230 ok')
            elif cmd == 'PASS': send('230 ok')
            elif cmd == 'PWD': send(f'257 "{cwd}"')
            elif cmd == 'CWD':
                v, r = real(cwd, arg)
                if v in [x for x in os.environ.get('DENY','').split(',') if x]: send('550 Permission denied')
                elif os.path.isdir(r): cwd = v; send('250 OK')
                else: send('550 no')
            elif cmd == 'TYPE': send('200 ok')
            elif cmd == 'FEAT' and os.environ.get('FEATZ'):    # wie zftpd: kennzeichnet sich in der FEAT-Liste
                send('211-Extensions supported:'); send('211- SIZE'); send('211- EPSV'); send('211- XZFTPD INSTANCE 0123456789abcdef'); send('211 End')
            elif cmd == 'NOOP': send('200 NOOP OK')
            elif cmd == 'EPSV' and os.environ.get('NOEPSV'): send('502 EPSV not implemented')   # wie manche FTP-Server
            elif cmd == 'PASV' and os.environ.get('NOPASV'): send('502 PASV not implemented')
            elif cmd in ('EPSV', 'PASV'):
                pasv = socket.socket(); pasv.bind(('127.0.0.1', 0)); pasv.listen(1)
                p = pasv.getsockname()[1]
                if cmd == 'EPSV': send(f'229 Entering Extended Passive Mode (|||{p}|)')
                else: send(f'227 Entering Passive Mode (127,0,0,1,{p>>8},{p&255})')
            elif cmd in ('LIST', 'NLST'):
                v, r = real(cwd, arg) if arg and not arg.startswith('-') else (cwd, ROOT + cwd)
                if os.environ.get('SLOW') and 'langsam' in r: time.sleep(10)
                send('150 open'); d, _ = pasv.accept()
                if os.environ.get('FAILLIST') and v == os.environ['FAILLIST'] and os.path.exists('faillist.on'):
                    d.sendall(b'drw'); d.close(); pasv.close(); send('451 read error'); continue
                names = ['.', '..'] + sorted(os.listdir(r))
                out = '\r\n'.join(n if cmd == 'NLST' else ls_line(os.path.join(r, n), n) for n in names) + '\r\n'
                d.sendall(out.encode()); d.close(); pasv.close(); send('226 done')
            elif cmd == 'SIZE':
                v, r = real(cwd, arg)
                send(f'213 {os.path.getsize(r)}') if os.path.isfile(r) else send('550 no')
            elif cmd == 'RETR':
                v, r = real(cwd, arg)
                if not os.path.isfile(r): send('550 no'); continue
                send('150 open'); d, _ = pasv.accept(); d.sendall(open(r,'rb').read()); d.close(); pasv.close(); send('226 done')
            elif cmd == 'STOR':
                v, r = real(cwd, arg)
                if os.environ.get('STOPFAIL') and v.endswith('/STOP'): send('550 Permission denied'); continue
                send('150 open'); d, _ = pasv.accept(); data = b''
                while True:
                    ch = d.recv(65536)
                    if not ch: break
                    data += ch
                d.close(); pasv.close(); open(r, 'wb').write(data); send('226 done')
            elif cmd == 'SITE':
                sub, _, rest = arg.partition(' ')
                if sub.upper() == 'CHMOD':
                    mode, _, path = rest.partition(' '); v, r = real(cwd, path)
                    if os.path.exists(r): os.chmod(r, int(mode, 8)); send('200 OK')
                    else: send('550 no')
                else: send('502 no')
            elif cmd in ('DELE', 'RMD', 'RMDA') and os.environ.get('FAILDEL') and real(cwd, arg)[0].startswith(os.environ['FAILDEL']):
                send('550 Permission denied')
            elif cmd == 'DELE':
                v, r = real(cwd, arg)
                if fake(r): shutil.rmtree(r); send('250 OK')
                elif os.path.islink(r) or os.path.isfile(r): os.remove(r); send('250 OK')
                else: send('550 no')
            elif cmd == 'RMD':
                v, r = real(cwd, arg)
                try: os.rmdir(r); send('250 OK')
                except Exception: send('550 Directory not empty')
            elif cmd == 'RMDA':
                if os.environ.get('RMDA') == '1':
                    v, r = real(cwd, arg)
                    if os.environ.get('SLOWRMDA') and 'langsam' in r: time.sleep(8)   # wie eine riesige Spieldatei
                    shutil.rmtree(r); send('250 OK')
                else: send('502 not implemented')
            elif cmd == 'MKD':
                v, r = real(cwd, arg); os.mkdir(r); send(f'257 "{v}"')
            elif cmd == 'RNFR':
                v, rnfr = real(cwd, arg); send('350 ok') if os.path.exists(rnfr) else send('550 no')
            elif cmd == 'RNTO':
                v, r = real(cwd, arg); os.rename(rnfr, r); send('250 OK')
            elif cmd == 'QUIT': send('221 bye'); break
            else: send('502 no')
        except Exception as e:
            send('550 ' + str(e))
    c.close()
def fake_shadowmount():
    # simuliert ShadowMount+: holt STOP ab, beendet sich 3 s spaeter
    sm = ROOT + '/user/data/shadowmount'
    while True:
        time.sleep(1)
        if os.path.isfile(sm + '/STOP'):
            os.remove(sm + '/STOP'); time.sleep(3)
            if os.environ.get('FAKESM') == 'stumm':      # Protokoll abgeschaltet: Spiele aushängen statt Log-Zeile
                mp = ROOT + '/mnt/shadowmnt'
                if os.path.isdir(mp):
                    for n in os.listdir(mp): shutil.rmtree(os.path.join(mp, n)); os.mkdir(os.path.join(mp, n))
            else:
                open(sm + '/debug.log', 'a').write('[12:00:00] [SHUTDOWN] cleanup complete for STOP file /data/shadowmount/STOP\n')
if os.environ.get('FAKESM') in ('1', 'stumm'): threading.Thread(target=fake_shadowmount, daemon=True).start()
s = socket.socket(); s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1); s.bind(('127.0.0.1', PORT)); s.listen(20)
while True:
    c, _ = s.accept(); threading.Thread(target=handle, args=(c,), daemon=True).start()
