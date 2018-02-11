# Astonia 3 & Astonia 3.5
Full mmorpg game source code, written by Daniel Brockhaus for Intent Software.

## What is Astonia?

Astonia (tm) is a massively multiplayer online role playing game (in short, MMORPG). Each player assumes the role of a battle-hardened warrior or a powerful mage. He or she enters the world of Astonia and begins to train for battle. The experience gained as characters solve quests, discover new places, and kill evil monsters, can be applied to raise any of a large number of attributes, skills and spells. Characters gain rank and power, qualifying them for better and more powerful equipment for the new and greater challenges that lie ahead...

### Installing

#### Step 1. Obtaining a VPS
 
In order to run an astonia server, you need a linux operating system. You have three choices here, 1. you can host it on your own computer if you run linux, 2. you can virtualize a linux os using virtualbox/vmware/hypervm etc., or the preferred option 3. You can rent a VPS (Or dedicated server if you plan on 1000+ players) from a company. I recommend https://patollic.com/cart.php?gid=3  since Ugaris is hosted by this company (Owned by me, Eddow). After ordering a VPS (Which is what this guide will assume) you recieve an IP address and a username & password combination. These are required to connect to your VPS using SSH. This bring us to ->
 
#### Step 2. Connecting to your server using SSH
 
To connect to our newly bought server, we will use Putty (free software for windows to connect to a server using SSH, available here:https://www.chiark.greenend.org.uk/~sgtatham/putty/latest.html . Download and install the 32 bit (Windows Installer) file and install and open the "putty" program.
 
It should look like this:
![Putty configuration](https://i.imgur.com/SYoCIFb.png "Putty configuration")
 
Put the IP address in the field pointed by arrow 1, and click on open pointed by arrow 2.
 
This will bring up a big black scary box called the terminal.It will ask for login, fill in your username (Most likely "root", and press enter, It now asks for a password, type in the password you received, It will not display anything on the screen, but it will register it, so don't worry, type the password and click enter. You should now see something like this:
![](https://i.imgur.com/XjHgofy.png "")
 
 From this box we will be executing a bunch of commands, and eventually "control" and do everything we need to run astonia. I will be listing a bunch of commands bellow, so make sure you type them all in exactly (Remember it is case sensitive). This brings us to ->
 
#### Step 3. Configuring the VPS to be "Astonia Ready"
 
To make the VPS astonia ready, we need to install some packages, edit some rules, make a user and all sorts of stuff. Luckily, I did most of the work for you, and all you have to do is copy/paste and press enter on the lines bellow. I will explain in detail what each line does however, to make you feel at ease, and so you can learn how and why =).
 
First command we will execute is
 ```Shell
 yum -y update
 ```
 to make sure our VPS is up to date. Don't fear! this will spout out a lot of text and might take some time, Wait until it is completed (up to 30-60 min depending on VPS)Your screen should look approximately like this: ![](https://i.imgur.com/jxxKQeT.png "") . Time to execute the next command:
```Shell
 yum -y groupinstall 'Development Tools'
```
 . This will install the nessecary files to "Build" the astonia server. Again this may take a while, just wait until it finishes.
 
Next up we install some libraries required by Astonia to build using the command:
```Shell 
yum -y install glibc.i686 glibc-devel.i686 libstdc++.i686 libstdc++-devel.i686 zlib.i686 zlib-devel.i686 mysql-devel.i686 mysql-libs.i686 mysql libpng mariadb-devel.i686 mariadb-devel psmisc
``` 
Next we need Mysql(mariadb) for the "database" which stores all information about accounts and characters.
```Shell 
yum -y install mariadb-server;
``` 
(This might already be installed and give an error saying "No package mysql-server available" in that case, move to next command.)
 
And start the mysql server
```Shell 
systemctl start mariadb.service
``` 
And ironically, we can shut it down right after
```Shell 
systemctl stop mariadb.service
``` 
Now we will update the mysql root password to the one used by astonia, this is very bad practise and should be avoided, but to keep things simple, we'll do it here. Might cover a better alternative in the future in a different tutorial. Run this command:
```Shell
/usr/bin/mysql -u root -e "UPDATE mysql.user SET Password=PASSWORD('flni1sbu') Where User='root'; FLUSH PRIVILEGES;" ;
```
 
And we can start the mysql server again (and leave it on this time) using:
```Shell 
systemctl start mariadb.service
``` 
Now we will make a "user account" for the astonia server on the vps.
```Shell 
useradd astonia
``` 
Now we have to move ourself to the astonia "home" folder using:
```Shell 
cd /home/astonia/
``` 
On to step 4 ->
Note: We will be executing commands as the "astonia" user by using sudo -u astonia infront of the commands. This is extremely important to prevent permission issues.
 
#### Step 4. Downloading Astonia server files
 
We will now download the files directly from brockhaus.org (If this link ever gets removed, replace it with a new link of the astonia 3 server files)
```Shell 
sudo -u astonia wget http://brockhaus.org/astonia3_server.tgz
``` 
Extract the archive:
```Shell 
sudo -u astonia tar -xzvf astonia3_server.tgz
``` 
Move ourselves in the actual astonia server folder
```Shell 
cd astonia3_server
``` 
Time to import the default mysql databases and content.
 
First we make a quick fix to the sql files
```Shell
replace "TYPE=MyISAM" "ENGINE=MYISAM" -- create_tables.sql
``` 
Next we make the tables
```Shell
mysql -uroot -pflni1sbu < create_tables.sql
```
And last import the default data (and ishtar character for later on)
```Shell
mysql -uroot -pflni1sbu merc < storage.sql
```
and
```Shell
mysql -uroot -pflni1sbu merc < merc.sql
``` 
Now we need to fix some old code to make the server run on 64bit.
Remove lines 23 to 25 from tool.h
```Shell
sed -i '23,25d' tool.h
```
Remove lines 83 to 94 from tool.c
```Shell
sed -i '83,97d' tool.c
```
Last commands to execute(Building the server) and reboot the system, to finalize
```Shell 
sudo -u astonia make
```
This one will take a while, just let it go, it will throw a bunch of errors and what not, this is normal. So long as the final result is like this:
![](https://i.imgur.com/3ZVLSF4.png "")
 
Make sure mysql starts when the server boots up
```Shell
chkconfig mariadb on
```
And Reboot!
```Shell
reboot
```
 
If all went well and you got all the way here, YAY! You are almost done!
 
This will disconnect us from the terminal, we have to wait 5 min or so, and re-connect using step 1. It should look like:
![](https://i.imgur.com/D2ndqpm.png "")
 
All that is left is to start the server, But first we have to edit one simple file in step 5.
 
 
#### Step 5. Connecting to the server for easier editing using FileZilla
If you plan on modifying any files, keep in mind you will always need to execute *make* after any modification to a .c or .h file. Because you will need to rebuild the game server.
 
To connect to your server using filezilla, you need to give the "astonia" user a password. You can do so by executing:
```Shell
passwd astonia
```

Type in the password, and press enter. Again this will most likely be invisible. You will have to repeat it.
 
Now you can download filezilla (free ftp program that allows you to connect using sftp (https://filezilla-project.org/download.php)
Install and open filezilla it should look like this:
![](https://i.imgur.com/gY11Cjw.png "")
Fill in:
1. your servers IP address
2. "astonia"
3. Password for astonia you just made
4. 22 (Very important you put 22 here, otherwise it won't connect)
 
Press connect, and on the right side you will see the server files. You can now use filezilla to edit the files. Right click a file and click edit. I highly suggest installing notepad++ (https://notepad-plus-plus.org/) and make it your default editor (google how to, im tired of typing xD)
 
Edit start file, and make sure that all areas are started. Just replace all of the text with this: https://pastebin.com/kyYFMmt7
 
 
Now boot up the server using:
First CD into the directory
```Shell
cd /home/astonia/astonia3_server/
```
then START!
```Shell
sudo -u astonia sh start
```
## Running Server Examples
* Ugaris - The legend recovered (https://www.ugaris.com) - V3
* Astonia 3 Resurrection (https://www.a3res.com) - V3
* Astonia 3 Invicta (https://www.a3inv.com) - V3
* Astonia 3 - Old World (http://www.a3oldworld.com) - V3
* Astonia Reborn 3.5 (http://www.astoniareborn.com/) - V3.5
* Aranock Online (http://www.aranockonline.com/) - V2
## Authors

* **Daniel Brockhaus** - *Full source* - [brockhaus.org](https://brockhous.org)
* **Edwin de Jong** - *Patches, support, and Tutorial* - [edwindejong.net](https://edwindejong.net) - [eddoww](https://github.com/eddoww)

## License

### Hobby License
You may use the software as you see fit, within the bounds of the law etc., provided you:

a) give proper credit (eg. "is based on the Astonia 3 engine by Intent Software" somewhere in your game / on your website).

b) generate no more than US$12,000 revenue per year

### Commercial License
This license only applies once you are earning more than US$12,000 per year. Once this is the case, please contact me me at daniel (at) brockhaus (dot) org.

You may use the software as you see fit, within the bounds of the law etc., provided you:

a) give proper credit (eg. "is based on the Astonia 3 engine by Intent Software" somewhere in your game / on your website).

b) pay Intent Software 10% of the revenues, once per month, 30 days after the end of the month.

## Third party Tools

PHP based website for account creation (https://github.com/eddoww/Astonia-Basic-Website)
## Acknowledgments

* Roman Haas, for always supporting and being there.
* ItsJustMe (deka) for inspiration of writing this
* Every single soul in "empire" back in the day. Love you all!
