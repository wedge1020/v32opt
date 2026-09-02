#/usr/bin/env bash

cat src/v32opt.h                                             >  put/v32opt.h

echo "//"                                                    >  put/v32opt.c
echo "// v32opt - Vircon32 assembler optiomizer written "    >> put/v32opt.c
echo "//          in C"                                      >> put/v32opt.c
echo "//"                                                    >> put/v32opt.c
echo "/////////////////////////////////////////////////////" >> put/v32opt.c
echo                                                         >> put/v32opt.c
echo '#include "v32opt.h"'                                   >> put/v32opt.c
echo                                                         >> put/v32opt.c
for src in `/bin/ls -1 src/*.c src/peephole/*.c`; do
    echo "// =========================================="     >> put/v32opt.c
    file=$(echo "${src}" | cut -d '/' -f2)
    echo "// ${file}"                                        >> put/v32opt.c
    echo "// =========================================="     >> put/v32opt.c
    cat ${src} | grep -v '#include'                          >> put/v32opt.c
    echo                                                     >> put/v32opt.c
done

exit 0
