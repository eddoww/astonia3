int is_badpass_ip(MYSQL *mysql,unsigned int ip);
void add_badpass_ip(MYSQL *mysql,unsigned int ip);
void clean_badpass_ips(MYSQL *mysql);
void clean_createpass_ips(MYSQL *mysql);
void add_create_ip(MYSQL *mysql,unsigned int ip);
int is_create_ip(MYSQL *mysql,unsigned int ip);
