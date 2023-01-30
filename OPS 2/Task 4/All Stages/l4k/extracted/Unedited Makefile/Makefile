CC=gcc
CFLAGS= -std=gnu99 -Wall -g -lrt -lpthread -pthread
#TESTARGS = -t 50 -n 3 -r 0 -b 1
FILE1 = linear#don't include extensions	#FILE1 is the MAIN file, as in, it is the file which shares it's name with the executable output
#FILE2 = client
SOURCEFILES= ${FILE1}.c #${FILE2}.c
ALLTARGETS = ${FILE1} #${FILE2}
OUTFILES = ${FILE1}.out #${FILE2}.out


all : ${ALLTARGETS}

${FILE1}: ${FILE1}.c
	${CC} ${CFLAGS} ${FILE1}.c -o ${FILE1}.out

$linear: linear.c
	${CC} ${CFLAGS} ${FILE1}.c -o ${FILE1}.out
${FILE2}: ${FILE2}.c
	${CC} ${CFLAGS} ${FILE2}.c -o ${FILE2}.out
clean: 
	rm -f ${OUTFILES}
tar:
	@read -p "Enter current stage:" STAGE;\
	tar -cjf noorn.stage$$STAGE.tar.bz2 Makefile ${SOURCEFILES}
tarsubmit:
	@read -p "Enter current stage:" STAGE;\
	scp noorn.stage$$STAGE.tar.bz2 noorn@ssh.mini.pw.edu.pl:/home2/samba/sobotkap/unix/
submit:
	@read -p "Enter current stage:" STAGE;\
	tar -cjf noorn.stage$$STAGE.tar.bz2 Makefile ${SOURCEFILES};\
	scp noorn.stage$$STAGE.tar.bz2 noorn@ssh.mini.pw.edu.pl:/home2/samba/sobotkap/unix
verify:
	@read -p "Enter stage to verify:" STAGE;\
	ssh noorn@ssh.mini.pw.edu.pl ls -l /home2/samba/sobotkap/unix/noorn.stage$$STAGE.tar.bz2
emptyLolOloL:
#This was kept empty to avoid overflowing of verify due to lots of comments following 		#this part
#test:
#	./${FILE1}.out ${TESTARGS}
#debugfs:
#	${CC} ${CFLAGS} -g -fsanitize=address ${FILE1}.c -o ${FILE1}.out
#	./${FILE1}.out ${TESTARGS}
#valgrind:
#	${CC} ${CFLAGS} -g ${FILE1}.c -o ${FILE1}.out
#	valgrind ./${FILE1}.out ${TESTARGS}
#	#or use "valgrind --leak-check=full ........" (check online to see what that does) or #maybe create another target/category for that below here
