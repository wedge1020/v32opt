#/usr/bin/env bash

# Concatenate the modular sources (and the split inc/ headers, in
# dependency order) into a single-file v32opt.h / v32opt.c pair under
# put/, for convenient single-compilation-unit experimentation.

# v32opt.h is the umbrella header; its system includes must come first in
# the concatenated single header, then the section headers in include
# order, then the rest of the umbrella (pass registry, config, trigger
# API) with all of its own #include lines stripped.
{
    grep '^#include <' inc/v32opt.h
    cat inc/asm.h inc/peephole.h inc/dataflow.h inc/inline.h inc/stack.h inc/promote.h
    grep -v '^#include' inc/v32opt.h
}                                                              >  put/v32opt.h

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
