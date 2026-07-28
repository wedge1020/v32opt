#!/usr/bin/env bash
##
## run_tests.sh - a script to automate the unit tests in the v32opt
##                assembly optimizer on its optimizations
##
#########################################################################

#set -x # enable debug mode
UNIT="${1}"
MODE="${2}"
[ -z "${MODE}" ] && MODE="verbose"

function out()
{
	[ "${MODE}" = "verbose" ] && echo "${1}"
}

printf "=== UNIT TEST REPORT (%s) " "${UNIT}"
len=$(echo "${UNIT}" | wc -L | tr -d ' ')
begin=$(((88-${len}+7)))
for ((index=${begin}; index < 89; index++)); do
	echo -n "="
done
echo

out "---file size comparison---------------------------------"
REGASMSIZE=$(ls  -l ${UNIT}.asm     | sed 's/  */ /g' | cut -d' ' -f5)
OPTASMSIZE=$(ls  -l ${UNIT}Opt.asm  | sed 's/  */ /g' | cut -d' ' -f5)
REGVBINSIZE=$(ls -l ${UNIT}.vbin    | sed 's/  */ /g' | cut -d' ' -f5)
OPTVBINSIZE=$(ls -l ${UNIT}Opt.vbin | sed 's/  */ /g' | cut -d' ' -f5)
ASMDIFF=$(echo "${REGASMSIZE}-${OPTASMSIZE}" | bc -q)
ASMPCT=$(echo  "${OPTASMSIZE}/${REGASMSIZE}" | bc -lq)
ASMPCT=$(echo  "${ASMPCT}*100"               | bc -lq)
ASMPCT=$(echo  "scale=3; ${ASMPCT} / 1"      | bc -lq)
ASMPCT=$(echo  "100.000-${ASMPCT}"           | bc -lq)
DECASMPCT=$(echo "${ASMPCT}" | cut -d'.' -f1)
[ "${DECASMPCT}" -lt 0 ] && QUAL="loss" || QUAL="savings"
printf "%-9s %5s bytes\n" "ASM:"    "${REGASMSIZE}"
printf "%-9s %5s bytes (%s%% %s)\n" "OPTASM:" "${OPTASMSIZE}" "${ASMPCT}" "${QUAL}"
#printf "%24s: %5s bytes "  "difference"     "${ASMDIFF}"
#printf "(%5s%%)\n"         "${ASMPCT}"
#ls -l ${UNIT}.asm    ${UNIT}.vbin
#ls -l ${UNIT}Opt.asm ${UNIT}Opt.vbin
VBINDIFF=$(echo "${REGVBINSIZE}-${OPTVBINSIZE}" | bc -q)
VBINPCT=$(echo  "${OPTVBINSIZE}/${REGVBINSIZE}" | bc -lq)
VBINPCT=$(echo  "${VBINPCT}*100"               | bc -lq)
VBINPCT=$(echo  "scale=3; ${VBINPCT} / 1"      | bc -lq)
VBINPCT=$(echo  "100.000-${VBINPCT}"           | bc -lq)
DECVBINPCT=$(echo "${VBINPCT}" | cut -d'.' -f1)
[ "${DECVBINPCT}" -lt 0 ] && QUAL="loss" || QUAL="savings"
printf "%-9s %5s bytes\n" "VBIN:"    "${REGVBINSIZE}"
printf "%-9s %5s bytes (%s%% %s)\n" "OPTVBIN:" "${OPTVBINSIZE}" "${VBINPCT}" "${QUAL}"

out "--------------------------------------------------------"
out
out "---MATCH results----------------------------------------"
ASMRESULT1=$(cat ${UNIT}.asm    | grep 'MATCH' | wc -l | tr -d ' ')
out "ASM: ${ASMRESULT1}"
OPTRESULT1=$(cat ${UNIT}Opt.asm | grep 'MATCH' | wc -l | tr -d ' ')
out "OPT: ${OPTRESULT1} (should be 0)"
if [ ! "${OPTRESULT1}" = "0" ]; then
	out "--------------------------------------------------------"
	out "!!! FALSE NEGATIVES: ${UNIT} missed expected targets"
fi
out "--------------------------------------------------------"

if [ ! "${OPTRESULT1}" = "0" ]; then
	out "---OPT MISSED MATCH-------------------------------------"
	cat ${UNIT}Opt.asm          | grep 'MATCH'
	out "--------------------------------------------------------"
fi

out
out "---KEEP results-----------------------------------------"
ASMRESULT2=$(cat ${UNIT}.asm    | grep 'KEEP' | wc -l | tr -d ' ')
out "ASM: ${ASMRESULT2}"
OPTRESULT2=$(cat ${UNIT}Opt.asm | grep 'KEEP' | wc -l | tr -d ' ')
out "OPT: ${OPTRESULT2} (should be identical)"
if [ ! "${ASMRESULT2}" = "${OPTRESULT2}" ]; then
	out "--------------------------------------------------------"
	out "!!! FALSE POSITIVES: ${UNIT} triggering on invalid data"
fi
out "--------------------------------------------------------"
out

if [ ! "${ASMRESULT2}" = "${OPTRESULT2}" ]; then
	out "---OPT TRIGGERED ON KEEP--------------------------------"
	cat ${UNIT}.asm             | grep 'KEEP'         >  asmkeep.tmp
	cat ${UNIT}Opt.asm          | grep 'KEEP'         >  optkeep.tmp
	diff asmkeep.tmp optkeep.tmp | grep '<'
	rm -f asmkeep.tmp optkeep.tmp
	out "--------------------------------------------------------"
fi

if [ "${OPTRESULT1}" = "0" ] && [ "${ASMRESULT2}" = "${OPTRESULT2}" ]; then
	out "${UNIT}: SUCCESSFULLY PASSED UNIT TESTS"
	status=0
else
	out "${UNIT}: FAILED UNIT TESTS"
	status=1
fi
out "========================================================"

exit ${status}
