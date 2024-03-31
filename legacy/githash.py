import os

print("getting git hash")
os.system("rm -f src/githash.h")
os.system("make src/githash.h")