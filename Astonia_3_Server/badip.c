/*
 *
 * $Id: badip.c,v 1.1 2006/09/22 09:55:46 devel Exp $ (c) 2006 D.Brockhaus
 *
 * $Log: badip.c,v $
 * Revision 1.1  2006/09/22 09:55:46  devel
 * Initial revision
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>
#include <math.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <mysql/mysql.h>
#include <mysql/mysqld_error.h>
#include "badip.h"

static int get_count(MYSQL *mysql,unsigned int ip,int timeout)
{
	MYSQL_RES *result;
	MYSQL_ROW row;
	char buf[256];
	int cnt;

	sprintf(buf,"select count(*) from badip where IP=%u and t>=%d",ip,(int)(time(NULL)-timeout));

	if (mysql_query(mysql,buf)) {
		printf("select: Error: %s (%d)",mysql_error(mysql),mysql_errno(mysql));
		return 42;
	}

	if (!(result=mysql_store_result(mysql))) {
		printf("store: Error: %s (%d)",mysql_error(mysql),mysql_errno(mysql));
		return 42;
	}

	if (!(row=mysql_fetch_row(result))) return 42;
	cnt=atoi(row[0]);
	mysql_free_result(result);

	return cnt;
}

int is_badpass_ip(MYSQL *mysql,unsigned int ip)
{
        int cnt;

	cnt=get_count(mysql,ip,60);
	if (cnt>3) return 1;

	cnt=get_count(mysql,ip,60*60);
	if (cnt>8) return 1;

	cnt=get_count(mysql,ip,60*60*24);
	if (cnt>25) return 1;

	return 0;
}

void add_badpass_ip(MYSQL *mysql,unsigned int ip)
{
	char buf[256];

	sprintf(buf,"insert badip values(%u,%d)",ip,(int)time(NULL));
	if (mysql_query(mysql,buf)) {
		printf("select: Error: %s (%d)",mysql_error(mysql),mysql_errno(mysql));
		return;
	}
}

void clean_badpass_ips(MYSQL *mysql)
{
	char buf[256];

	sprintf(buf,"delete from badip where t<%d",(int)(time(NULL)-60*60*24*7));
	if (mysql_query(mysql,buf)) {
		printf("select: Error: %s (%d)",mysql_error(mysql),mysql_errno(mysql));
		return;
	}
}

/*

create table badip (IP int unsigned not null, t int not null, key lookup(IP,t), key(t));

*/
