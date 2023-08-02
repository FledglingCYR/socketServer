/********************************************************************************
**  (C) COPYRIGHT 2018 
**	author	:	HuangZiBin
**  E-mail	:	635568706@qq.com
** 	version	:	1.0.1
**	If you find bugs, please contact me.Thank you!
********************************************************************************/
//该程序出自man
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>


//if is_read > 0, do the read select, else write select
static int my_select(int fd, int timeout_ms, int is_read)
{
	fd_set rfds;
	struct timeval tv;
	int retval = 0;

	FD_ZERO(&rfds);
	FD_SET(fd, &rfds);

	tv.tv_sec = timeout_ms/1000;
	tv.tv_usec = timeout_ms%1000*1000;
	
	//if tv is null, timeout is infintely.if tv's value is 0, select return immediately
	if (is_read > 0) {
		retval = select(fd+1, &rfds, NULL, NULL, &tv);
	}else {
		retval = select(fd+1, NULL, &rfds, NULL, &tv);
	}
	if (retval == -1) {
		if (errno == EINTR) {
			return 0;
		}			
		//fprintf(stderr, "select failed, error:%s\r\n", strerror(errno));
	}
	/*
	else if (retval)
		printf("Data is available now.left time:%ldms\n", tv.tv_sec*1000+tv.tv_usec/1000);
	*/
	return retval;
}

//return: <0 means an error happened; 0 means nothing can be read from fd; >0 means something can be read
int my_read_select(int fd, int timeout_ms)
{
	return my_select(fd, timeout_ms, 1);
}

//return: <0 means an error happened; 0 means nothing can be write to fd; >0 means something can be write
int my_write_select(int fd, int timeout_ms)
{
	return my_select(fd, timeout_ms, 0);
}
#if 0
int main()
{
	int ret = my_read_select(STDIN_FILENO, 2000);
	if (ret < 0)
		return -1;
	else if (ret)
		printf("Data is available now.\n");
		/* FD_ISSET(0, &rfds) will be true. */
	else
		printf("No data within %d mseconds.\n", 2000);
	return 0;
}
#endif
#if 0
int main(void)
{
   fd_set rfds;
   struct timeval tv;
   int retval;

   FD_ZERO(&rfds);
   FD_SET(STDIN_FILENO, &rfds);

   tv.tv_sec = 5;
   tv.tv_usec = 0;

   retval = select(STDIN_FILENO+1, &rfds, NULL, NULL, &tv);
   if (retval == -1)
       perror("select()");
   else if (retval)
       printf("Data is available now.left time:%ldms\n", tv.tv_sec*1000+tv.tv_usec/1000);
       /* FD_ISSET(0, &rfds) will be true. */
   else
       printf("No data within five seconds.\n");

   exit(EXIT_SUCCESS);
}
#endif
