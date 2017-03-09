
Current status
---
* Tested on NetBSD-7 with Tcl/Tk 8.6:
* Save works 

Installation
---
```shell
cd scotty/unix
./configure --prefix=/usr/local/scotty
make
make install     # as root or privileged user
make sinstall    # as root
TCLLIBPATH=/usr/local/scotty/lib /usr/local/scotty/bin/tkined1.5.0
```

Known bugs
---

