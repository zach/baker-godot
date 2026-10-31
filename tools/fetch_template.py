"""Installs Godot export templates without downloading the whole bundle.

    python3 tools/fetch_template.py [--mono] VERSION NAME...
    python3 tools/fetch_template.py --mono 4.7.2 linux_release.x86_64 windows_release_x86_64.exe

--mono: the .NET build's templates, which a project with C# exports with
(VERSION.stable.mono).

The templates come as one zip of over a gigabyte; this reads its directory and the named
members with HTTP range requests, and puts them where Godot looks for them
(~/.local/share/godot/export_templates/VERSION.stable, or %APPDATA%/Godot/... on
Windows).
"""
import io, os, sys, urllib.request, zipfile

class Remote(io.RawIOBase):
    def __init__(self, url):
        self.url, self.pos = url, 0
        with urllib.request.urlopen(urllib.request.Request(url, method="HEAD")) as r:
            self.size = int(r.headers["Content-Length"])
            self.url = r.url
    def seekable(self): return True
    def readable(self): return True
    def tell(self): return self.pos
    def seek(self, off, whence=0):
        self.pos = {0: off, 1: self.pos + off, 2: self.size + off}[whence]
        return self.pos
    def read(self, n=-1):
        if n is None or n < 0: n = self.size - self.pos
        if n == 0 or self.pos >= self.size: return b""
        end = min(self.pos + n, self.size) - 1
        req = urllib.request.Request(self.url, headers={"Range": f"bytes={self.pos}-{end}"})
        with urllib.request.urlopen(req) as r: data = r.read()
        self.pos += len(data)
        return data
    def readinto(self, b):
        d = self.read(len(b)); b[:len(d)] = d; return len(d)

def template_dir(version, mono):
    if os.name == "nt":
        base = os.path.join(os.environ["APPDATA"], "Godot")
    else:
        base = os.path.join(os.environ.get("XDG_DATA_HOME") or os.path.expanduser("~/.local/share"), "godot")
    return os.path.join(base, "export_templates", version + ".stable" + (".mono" if mono else ""))


def main():
    args = sys.argv[1:]
    mono = args[:1] == ["--mono"]
    if mono:
        args = args[1:]
    version, names = args[0], args[1:] + ["version.txt"]
    url = (f"https://github.com/godotengine/godot/releases/download/{version}-stable/"
           f"Godot_v{version}-stable{'_mono' if mono else ''}_export_templates.tpz")
    out = template_dir(version, mono)
    os.makedirs(out, exist_ok=True)
    z = zipfile.ZipFile(io.BufferedReader(Remote(url), buffer_size=1 << 20))
    for info in z.infolist():
        name = os.path.basename(info.filename)
        if name in names:
            with z.open(info) as src, open(os.path.join(out, name), "wb") as dst:
                dst.write(src.read())
            print(f"{name}: {info.file_size} bytes into {out}")


if __name__ == "__main__":
    main()
