import sys
import subprocess

# git rev-parse --short HEAD | tr [:lower:] [:upper:]

res = subprocess.check_output(['git', 'rev-parse', '--short', 'HEAD'])
res = res.decode('utf-8')
res = res.strip().upper()

fname = sys.argv[1]
f = open(fname, "w")
print("#pragma once", file=f)
print("const unsigned char _hash[] = \"" + str(res) + "\";", file=f)
f.close()