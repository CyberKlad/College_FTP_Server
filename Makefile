START = start
${START}:
	rm -f myftp myftpserve myftp.o myftpserve.o
	cc -c myftp.c
	cc -c myftpserve.c
	cc -o myftp myftp.o
	cc -o myftpserve myftpserve.o
