It is highly recommended at this point to download and install Notepad++, which you can safely download at: (https://notepad-plus-plus.org/download/)

### Configuring the Client

#### Step 1. Getting the client files

Since we are not using a remote desktop to do all this, these steps are quite a bit easier then installing the server. First we download the client files from either brockhaus.org or this github, and extract them in a folder. It should contain 3 folders (gfx, lib and moac).

Navigate to the moac folder and right click on client.c and select open with Notepad++, like this:

![Open with notepad++](https://i.imgur.com/nuqxEzD.png "Open with notepad++")

#### Step 2. Editing files.

Now before we edit some things, also make sure to open main.h, this is where the type of "client" will be adjusted. On line 3 to line 5 in main.h there are 3 define statements, with two being commented out.

![](https://i.imgur.com/jyHN7DS.png)
Developer means a development built of the client, which will display some additional logging ingame. Highly recommend this version of the client for staff/administration only, and even then is not nessecary.
Staffer means the "editor", don't ask me why, but that's what it is.
Hacker is something you can forget. It's some debugging left out.

So you have two choices, leaving line 3 the way it is (for debugging), or commenting it out (for public release). Depending on the choice you make, we need to edit some lines in client.c

![](https://i.imgur.com/0DSrQc1.png)

In the picture above, you need to edit arrow 1 for the developer version, and arrow 2 for the public version.
You have to change these values to reflect your IP address.

Here is an example, if your ip address is 123.456.789.000 You would change these lines like this:

```
int target_server=(123<<24)|(456<<16)|(789<<8)|(00<<0);
int update_server=(123<<24)|(456<<16)|(789<<8)|(00<<0);
int base_server=(123<<24)|(456<<16)|(789<<8)|(00<<0);
```

![](https://i.imgur.com/dCYDxjc.png)

Note that in the picture above I removed the `#` from the start of line 60, and I changed the IP address on both. This is usually the way to go at it.


There is one more part of the code, in client.c where you have to change these numbers. This part can be found between the line 897 and line 913. It's the same kind of edit as before, so I won't go into too much detail.

Old:
![](https://i.imgur.com/46wNQuF.png)

New with 123.456.789.000:

![](https://i.imgur.com/n9TzQL9.png)


After changing these lines, save the file and close notepad++.

#### Step 3. Compiling the client

Before we begin this step, make sure you have installed the Borland C++ 5.5 command line tools available from brockhaus.org and github ((http://www.brockhaus.org/freecommandLinetools.exe)here or here(https://github.com/eddoww/astonia3/raw/master/Extra/Borland%20Command%20Line%20Tools/freecommandLinetools%20(1).exe))

Open a command prompt (hold the windows key, press R, a window popups on the bottom of screen like this:

![](https://i.imgur.com/AWu59xb.png)

Type in CMD, and press enter. 

Change to the directory you placed the source files, In my case it is in "D:\Development\astonia3\Astonia_3_Client\moac" but this will be diffirent for you, depending where you unpacked the client files.

![](https://i.imgur.com/aJhyXBl.png)

Now type

`make`

And this will compile the client for you. It should look approximately like this:
![](https://i.imgur.com/t5KlT5Y.png)


#### Step 4. Copying the .pak files and making a game directory

In order to use the newly created .exe file in the previous step, it will need the resources/images of the game. Luckily these are all pre-packed and compiled, so it will be a simple drag and drop from the gfx folder, to a new folder that we'll call "game". 

Copy all of these: 
![](https://i.imgur.com/jr8EztU.png)

And the moac.exe and sound.pak from the previous step:

![](https://i.imgur.com/DoPcfGp.png)

To the new folder we called "Game" (You can call this anything, but to make it easier on us, we call it game)

It should then contain all these files:
![](https://i.imgur.com/nSro46f.png)

#### Step 4. Play the game!

Depending on which version of the client you compiled (develop or non-develop), we should be able to open the new moac.exe directly. If you decided on the public release version, you will need to upload ALL THESE files to a folder on the server side. See server FAQ for more information on this.

Now that we have ourselves a running client, we can open it, and login with the default username/password (Ishtar - Rene757) and we should be able to login and play.

Have fun!
