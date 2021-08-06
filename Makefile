srcDir = src
testDir = test
binDir = bin

ifeq (${OS}, Windows_NT)
	outExe = denoise.exe
	testExe = ${binDir}/test/test.exe
else
	outExe = denoise
	testExe = ${binDir}/test/test
endif

commonObjects = ${binDir}/noise_reduction.o
mainObject = ${binDir}/main.o
testObjects = ${binDir}/test/test.o \
			  ${binDir}/test/quickcheck4c.o

flags = -Werror -Wall -std=c11

.DEFAULT_GOAL = ${outExe}

${outExe} : ${commonObjects} ${mainObject}
	${CC} -o $@ ${flags} $^ -l portaudio

${binDir}:
	mkdir ${binDir}

${binDir}/%.o : ${srcDir}/%.c ${binDir}
	${CC} -c -o $@ ${flags} $<

.PHONY : test
test : ${testExe}
	./${testExe}

${testExe} : ${commonObjects} ${testObjects}
	${CC} -o $@ ${flags} $^

${binDir}/test: ${binDir}
	mkdir ${binDir}/test

${binDir}/test/%.o : ${testDir}/%.c ${binDir}/test
	${CC} -c -o $@ ${flags} -I${srcDir} $<

.PHONY : clean
clean:
	${RM} -r ${binDir}
	${RM} ${outExe}
