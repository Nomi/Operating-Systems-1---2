CC=gcc
CFLAGS= -std=gnu99 -Wall
TESTARGS = -t 50 -n 3 -r 0 -b 1
FILE1 = main#don't include extensions	#FILE1 is the MAIN file, as in, it is the file which shares it's name with the executable output
#FILE2=
FILES = ${FILE1}#${FILE2}
OUTFILES = ${FILE1}.out#${FILE2}.out


all : ${FILES}

${FILE1}: ${FILE1}.c
	${CC} ${CFLAGS} ${FILE1}.c -o ${FILE1}.out
#${FILE2}: %{FILE2},c
#	${CC} ${CFLAGS} ${FILE1}.c -o ${FILE2}.out
clean: 
	rm -f ${OUTFILES}
tar:
	@read -p "Enter current stage:" STAGE;\
	tar -cjf noorn.stage$$STAGE.tar.bz2 Makefile ${FILE1}.c
tarsubmit:
	@read -p "Enter current stage:" STAGE;\
	scp noorn.stage$$STAGE.tar.bz2 noorn@ssh.mini.pw.edu.pl:/home2/samba/sobotkap/unix/
submit:
	@read -p "Enter current stage:" STAGE;\
	tar -cjf noorn.stage$$STAGE.tar.bz2 Makefile ${FILE1}.c;\
	scp noorn.stage$$STAGE.tar.bz2 noorn@ssh.mini.pw.edu.pl:/home2/samba/sobotkap/unix/
test:
	./${FILE1}.out ${TESTARGS}
valgrind:
	valgrind ./${FILE1}.out ${TESTARGS}
	#or use "valgrind --leak-check=full ........" (check online to see what that does) or maybe create another target/category for that below here
