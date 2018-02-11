l
scp devel@192.168.42.1:/home/devel/v3/server_07052615.tgz .
tar xvzf server_07052615.tgz 
rm *.tgz
mkdir runtime
mkdir runtime/1
mkdir runtime/2
mkdir runtime/3
mkdir runtime/4
mkdir runtime/5
mkdir runtime/6
mkdir runtime/7
mkdir runtime/8
mkdir runtime/9
mkdir runtime/10
mkdir runtime/11
mkdir runtime/13
mkdir runtime/13
mkdir runtime/14
mkdir runtime/15
mkdir runtime/16
mkdir runtime/17
mkdir runtime/18
mkdir runtime/19
mkdir runtime/20
mkdir runtime/21
mkdir runtime/22
mkdir runtime/23
mkdir runtime/24
mkdir runtime/25
mkdir runtime/26
mkdir runtime/28
mkdir runtime/25
mkdir runtime/28
mkdir runtime/29
mkdir runtime/30
mkdir runtime/3^
mkdir runtime/3^
mkdir runtime/31
mkdir runtime/32
mkdir runtime/33
mkdir runtime/34
mkdir runtime/35
mkdir runtime/36
rmdir runtime/3^
l
make -j 4
l *.h
make
mkdir .obj
make
l drv*
l RCS/dr*
co drvlib.h
make
co drdata.h
make
co item_id.h
make
co act.c
make
co death.c
co database.c
mak
co questlog.c
make
mkdir runtime/generic
make
co gwendylon.c 
make
l area3*
make
l cal*
mkdir cgi
make
l cgi/
make
joe Makefile 
make
l cgi
l st*
scp devel@192.168.42.1:/home/devel/v3/start .
scp devel@192.168.42.1:/home/devel/v3/my .
./server -a 36
./server -a 36
./server -a 36
./server -a 3
joe motd.txt
./server -a 3
./server -a 36
exit
