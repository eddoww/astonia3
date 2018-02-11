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
#define EXTERNAL_PROGRAM
#include "server.h"
#undef EXTERNAL_PROGRAM
#include "mail.h"
#include "statistics.h"
#include "clan.h"
#include "drdata.h"
#include "skill.h"
#include "depot.h"
#include "club.h"
#include "bank.h"
#include "badip.h"

static MYSQL mysql;
static char mysqlpass[80];

static void makemysqlpass(void)
{
        static char key1[]={117, 127, 98, 38, 118, 115, 100, 104,0};
        static char key2[]={"qpc74a7v"};
        static char key3[]={"bcoxsa1k"};
        int n;

        for (n=0; key1[n]; n++) {
                mysqlpass[n]=key1[n]^key2[n]^key3[n];
                //printf("%d, ",mysqlpass[n]);
        }
        mysqlpass[n]=0;
}
  
static void destroymysqlpass(void)
{
        bzero(mysqlpass,sizeof(mysqlpass));
}

int init_database(void)
{
        // init database client
        if (!mysql_init(&mysql)) return 0;
        
        // try to login as root with our password
        makemysqlpass();
        if (!mysql_real_connect(&mysql,"localhost","root",mysqlpass,"mysql",0,NULL,0)) {
                destroymysqlpass();
                fprintf(stderr,"MySQL error: %s (%d)\n",mysql_error(&mysql),mysql_errno(&mysql));
                return 0;
        }
        destroymysqlpass();

        if (mysql_query(&mysql,"use merc")) {
                fprintf(stderr,"MySQL error: %s (%d)\n",mysql_error(&mysql),mysql_errno(&mysql));
                return 0;
        }

        return 1;
}

void exit_database(void)
{
	mysql_close(&mysql);
}

int main(int argc,char **args)
{
	char buf[256];

	if (argc!=3) {
		fprintf(stderr,"Usage: %s <email> <password>\n",args[0]);
		return 1;
	}
	
	if (!init_database()) {
	        fprintf(stderr,"Cannot connect to database.\n");
	        return 3;
        }

	sprintf(buf,"insert subscriber (email,password,creation_time,locked,banned,vendor) values ("
                "'%s',"         // email
                "'%s',"         // password
                "%d,"           // creation time
                "'N',"          // locked
                "'I',"          // banned
                "%d)",          // vendor
                args[1],args[2],(int)time(NULL),0);
                
        if (mysql_query(&mysql,buf)) {
                fprintf(stderr,"Failed to create subscriber: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));
                return 2;
        }

	printf("Success. Account ID is %d.\n",(int)mysql_insert_id(&mysql));
	
	exit_database();
	
	return 0;
}

