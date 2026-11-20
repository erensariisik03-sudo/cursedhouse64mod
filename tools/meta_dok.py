#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
meta_dok.py - Unity IL2CPP global-metadata.dat dosyasini yapay zekanin okuyabilecegi
duz metne cevirir (sadece standart kutuphane, kurulum gerekmez).

Cikti: sinif / alan / metot / parametre ADLARI ve istege bagli string literal'ler.
(Tip adlari ve RVA adresleri libil2cpp.so + Il2CppDumper ile gelir; bu script onlari uretmez.)

Kullanim:
  python meta_dok.py global-metadata.dat                  -> global-metadata.siniflar.txt
  python meta_dok.py global-metadata.dat --ara InputField -> sadece "InputField" gecen siniflar
  python meta_dok.py global-metadata.dat --ara chat --parca 24
  python meta_dok.py global-metadata.dat --dizeler        -> string literal'leri de yazar
  python meta_dok.py global-metadata.dat --ozet           -> sadece istatistik
Not: --ara bir siniflar blogunun ICINDE (sinif/alan/metot/parametre adi) arar, buyuk/kucuk harf farketmez.
"""
import argparse
import json
import os
import struct
import sys

TYPE_SZ, METH_SZ, FIELD_SZ, PARAM_SZ = 88, 36, 12, 12


def parcala(bloklar, taban, kb, sonek):
    limit = kb * 1024
    parcalar, cur, boyut = [], [], 0
    for b in bloklar:
        if cur and boyut + len(b) > limit:
            parcalar.append(cur)
            cur, boyut = [], 0
        cur.append(b)
        boyut += len(b) + 1
    if cur:
        parcalar.append(cur)
    for i, p in enumerate(parcalar, 1):
        ad = f"{taban}.{sonek}.parca{i:02d}.txt"
        with open(ad, "w", encoding="utf-8") as f:
            f.write(f"# PARCA {i}/{len(parcalar)}\n" + "\n".join(p) + "\n")
        print("yazildi:", ad)


def yaz(bloklar, taban, kb, sonek):
    if kb:
        parcala(bloklar, taban, kb, sonek)
        return
    ad = f"{taban}.{sonek}.txt"
    with open(ad, "w", encoding="utf-8") as f:
        f.write("\n".join(bloklar) + "\n")
    print(f"yazildi: {ad}  ({os.path.getsize(ad) // 1024} KB)")


def main():
    ap = argparse.ArgumentParser(description="IL2CPP global-metadata.dat -> okunur metin")
    ap.add_argument("dosya")
    ap.add_argument("--ara", help="icinde bu kelime gecen siniflari (ve --dizeler ile literal'leri) sec")
    ap.add_argument("--dizeler", action="store_true", help="string literal'leri de cikar")
    ap.add_argument("--parca", type=int, metavar="KB", help="KB boyutunda parcalara bol")
    ap.add_argument("--ozet", action="store_true", help="sadece istatistik goster")
    ap.add_argument("-o", "--cikti", help="cikti on eki")
    a = ap.parse_args()

    d = open(a.dosya, "rb").read()
    magic, ver = struct.unpack("<Ii", d[:8])
    if magic != 0xFAB11BAF:
        sys.exit(f"Bu standart bir global-metadata degil (magic={d[:4].hex()}). "
                 "Oyun metadata'yi sifrelemis olabilir; once bellekten/decrypt edilmis halini al.")
    H = lambda i: struct.unpack("<ii", d[8 + i * 8:16 + i * 8])

    so, _ = H(2)
    s_cache = {}

    def S(i):
        if i < 0:
            return ""
        r = s_cache.get(i)
        if r is None:
            e = d.index(b"\0", so + i)
            r = s_cache[i] = d[so + i:e].decode("utf-8", "replace")
        return r

    to, ts = H(19)
    mo, ms = H(5)
    fo, fs = H(11)
    po, ps = H(10)
    if ts % TYPE_SZ or ms % METH_SZ or fs % FIELD_SZ or ps % PARAM_SZ:
        sys.exit(f"Surum {ver}: yapi boyutlari beklenenle uyusmuyor. Script v31 icin dogrulandi.")
    ntype = ts // TYPE_SZ
    print(f"IL2CPP metadata v{ver}: {ntype} tip, {ms // METH_SZ} metot, {fs // FIELD_SZ} alan")
    if ver != 31:
        print("UYARI: script v31 icin test edildi; cikti hatali olabilir.")

    taban = a.cikti or os.path.splitext(a.dosya)[0]
    anahtar = a.ara.lower() if a.ara else None

    siniflar = []
    for t in range(ntype):
        r = struct.unpack_from("<16i8H2I", d, to + t * TYPE_SZ)
        ad, ns = S(r[0]), S(r[1])
        flds, mths = r[8], r[9]
        mcount, fcount = r[16], r[18]
        satirlar = [f"class {ns + '.' if ns else ''}{ad}"]
        for i in range(fcount):
            fr = struct.unpack_from("<3i", d, fo + (flds + i) * FIELD_SZ)
            satirlar.append(f"  F {S(fr[0])}")
        for i in range(mcount):
            m = struct.unpack_from("<7i4H", d, mo + (mths + i) * METH_SZ)
            # m indeksleri: 0 ad,1 declType,2 ret,3 retParamToken,4 paramStart,5 genCont,6 token,
            #               7 flags,8 iflags,9 slot,10 paramCount
            pn = []
            for k in range(m[10]):
                pr = struct.unpack_from("<3i", d, po + (m[4] + k) * PARAM_SZ)
                pn.append(S(pr[0]))
            st = "static " if m[7] & 0x10 else ""
            satirlar.append(f"  M {st}{S(m[0])}({', '.join(pn)})")
        blok = "\n".join(satirlar)
        if anahtar is None or anahtar in blok.lower():
            siniflar.append(blok)

    print(f"{len(siniflar)} sinif secildi")
    if a.ozet:
        return
    yaz(siniflar, taban, a.parca, "siniflar")

    if a.dizeler:
        lo, ls = H(0)
        do, _ = H(1)
        lit = []
        for i in range(ls // 8):
            ln, di = struct.unpack_from("<II", d, lo + i * 8)
            txt = d[do + di:do + di + ln].decode("utf-8", "replace")
            if anahtar is None or anahtar in txt.lower():
                lit.append(f"{i}: {json.dumps(txt, ensure_ascii=False)}")
        print(f"{len(lit)} string literal secildi")
        yaz(lit, taban, a.parca, "dizeler")


if __name__ == "__main__":
    main()
