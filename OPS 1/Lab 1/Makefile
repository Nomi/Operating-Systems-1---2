#
### Welcome to my Makefile. Pretty neat, innit? ###
#
CC=gcc#or clang
CFLAGS= -std=gnu99 -Wall -g -fsanitize=address,undefined
#TESTARGS = -t 50 -n 3 -r 0 -b 1
FILE1 = fsdb#don't include extensions	#FILE1 is the MAIN file, as in, it is the file which shares it's name with the executable output
#FILE2 = file2name
SOURCEFILES= ${FILE1}.c #${FILE2}.c
ALLTARGETS = ${FILE1}.out #${FILE2}.out
OUTFILES = ${FILE1}.out #${FILE2}.out
TEACHERDIRECTORY= karwowskij
# Maybe create another parameter that sets teh programming language/extension for compilation commands?
#REMINDER: ctrl+/ is "comment" shortcut in VSCode


all : ${ALLTARGETS}

${FILE1}.out: ${FILE1}.c
	${CC} ${CFLAGS} ${FILE1}.c -o ${FILE1}.out

#${FILE2}.out: ${FILE2}.c
#	${CC} ${CFLAGS} ${FILE2}.c -o ${FILE2}.out

.PHONY: clean submit verify tar tarsubmit emptyLolOloL

clean: 
	rm -f ${OUTFILES}
submit:
	@read -p "Enter current stage:" STAGE;\
	tar -cjf noorn.stage$$STAGE.tar.bz2 Makefile ${SOURCEFILES};\
	cp noorn.stage$$STAGE.tar.bz2 /home2/samba/${TEACHERDIRECTORY}/unix
verify:
	@read -p "Enter stage to verify:" STAGE;\
	ls -l /home2/samba/${TEACHERDIRECTORY}/unix/noorn.stage$$STAGE.tar.bz2
tar:
	@read -p "Enter current stage:" STAGE;\
	tar -cjf noorn.stage$$STAGE.tar.bz2 Makefile ${SOURCEFILES}
tarsubmit:
	@read -p "Enter current stage:" STAGE;\
	cp noorn.stage$$STAGE.tar.bz2 /home2/samba/${TEACHERDIRECTORY}/unix/

#----OLD PARTS THAT I'D LIKE TO FLESH OUT MORE:----
#test:
#	./${FILE1}.out ${TESTARGS}
#debugfs:
#	${CC} ${CFLAGS} -g -fsanitize=address ${FILE1}.c -o ${FILE1}.out
#	./${FILE1}.out ${TESTARGS}
#valgrind:
#	${CC} ${CFLAGS} -g ${FILE1}.c -o ${FILE1}.out
#	valgrind ./${FILE1}.out ${TESTARGS}
#	#or use "valgrind --leak-check=full ........" (check online to see what that does) or #maybe create another target/category for that below here


#Made by Noman Noor (https://nomi.github.io)