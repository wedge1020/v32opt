#!/usr/bin/env bash
UNIT="${1}"

echo "===UNIT TEST REPORT========================================="
echo "---file size comparison-------------------------------------"
#@stat -c '%20n: %8s bytes' ${UNIT}.asm ${UNIT}Opt.asm
ls -l ${UNIT}.asm ${UNIT}Opt.asm
echo "------------------------------------------------------------"
echo
echo "---MATCH results--------------------------------------------"
ASMRESULT1=$(cat ${UNIT}.asm | grep 'MATCH' | wc -l)
echo "ASM: ${ASMRESULT1}"
OPTRESULT1=$(cat ${UNIT}Opt.asm | grep 'MATCH' | wc -l)
echo "OPT: ${OPTRESULT1} (should be 0)"
if [ ! "${ASMRESULT1}" = "${OPTRESULT1}" ]; then
	echo "------------------------------------------------------------"
	echo "!!! FALSE NEGATIVES: ${UNIT} missed expected targets"
fi
echo "------------------------------------------------------------"
echo

if [ ! "${ASMRESULT1}" = "${OPTRESULT1}" ]; then
	echo "---OPT MISSED MATCH-----------------------------------------"
	cat ${UNIT}Opt.asm | grep 'MATCH'
	echo "------------------------------------------------------------"
fi

echo
echo "---KEEP results---------------------------------------------"
ASMRESULT2=$(cat ${UNIT}.asm | grep 'KEEP' | wc -l)
echo "ASM: ${ASMRESULT2}"
OPTRESULT2=$(cat ${UNIT}Opt.asm | grep 'KEEP' | wc -l)
echo "OPT: ${OPTRESULT2} (should be identical)"
if [ ! "${ASMRESULT2}" = "${OPTRESULT2}" ]; then
	echo "------------------------------------------------------------"
	echo "!!! FALSE POSITIVES: ${UNIT} triggering on invalid data"
fi
echo "------------------------------------------------------------"
echo

if [ ! "${ASMRESULT2}" = "${OPTRESULT2}" ]; then
	echo "---OPT TRIGGERED ON KEEP------------------------------------"
	cat ${UNIT}.asm    | grep 'KEEP' >  asmkeep.tmp
	cat ${UNIT}Opt.asm | grep 'KEEP' >  optkeep.tmp
	diff asmkeep.tmp optkeep.tmp | grep '<'
	rm -f asmkeep.tmp optkeep.tmp
	echo "------------------------------------------------------------"
fi
#echo "---code differences-----------------------------------------"
#diff ${UNIT}.asm ${UNIT}Opt.asm
#echo "------------------------------------------------------------"

if [ "${ASMRESULT1}" = "${OPTRESULT1}" ] && [ "${ASMRESULT2}" = "${OPTRESULT2}" ]; then
	echo "${UNIT}: SUCCESSFULLY PASSED UNIT TESTS"
	status=0
else
	echo "${UNIT}: FAILED UNIT TESTS"
	status=1
fi

exit ${status}
