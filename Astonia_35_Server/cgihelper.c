/*
 * $Id: cgihelper.c,v 1.3 2007/10/29 13:57:04 devel Exp $ (c) 2007 D.Brockhaus
 *
 * $Log: cgihelper.c,v $
 * Revision 1.3  2007/10/29 13:57:04  devel
 * fixed urls
 *
 * Revision 1.2  2007/08/13 18:36:56  devel
 * *** empty log message ***
 *
 * Revision 1.1  2007/07/24 18:53:49  devel
 * Initial revision
 *
 * Revision 1.1  2007/07/13 15:47:16  devel
 * Initial revision
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <mysql/mysql.h>
#include <mysql/mysqld_error.h>

#define RANDOM(a)	(rand()%(a))

extern MYSQL mysql;

void onlinecount(void)
{
	MYSQL_RES *result;
	MYSQL_ROW row;
	int cnt;

        if (mysql_query(&mysql,"select val from merc1.vars where name='cur_on'")) {
                printf("select: Error: %s (%d)\n",mysql_error(&mysql),mysql_errno(&mysql));
                return;
	}

	if (!(result=mysql_store_result(&mysql))) {
		printf("store: Error: %s (%d)\n",mysql_error(&mysql),mysql_errno(&mysql));
		return;
	}

        if (!(row=mysql_fetch_row(result))) { mysql_free_result(result); return; }

        cnt=atoi(row[0]);

        mysql_free_result(result);

	printf("%d Online",cnt);
}

void teaser(void)
{
        printf("<a href=\"http://urd.astonia.com/cgi-v35/account.cgi?step=999\"><img src=\"/img/banner%d.jpg\" alt=\"\" height=\"250\" width=\"110\" border=\"0\"></a>",RANDOM(1)+1);
}

void screenshots(void)
{
        printf("<a href=\"http://urd.astonia.com/screenshots.shtml\"><img src=\"/screens/screen%03ds.jpg\" alt=\"\" height=\"82\" width=\"110\" border=\"0\"></a>",RANDOM(6)+1);
}











