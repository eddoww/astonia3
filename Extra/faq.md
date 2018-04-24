* How do I enable Livequests?

In order for livequests to work, you'll have to create a new folder 

* How do I use the auto-updater with the public release client?

You need to make a folder in your server folder named :"game". In this folder you need to place the latest moac.exe new.exe and .pak files.

* How do I make an installer for the game?

There are several different programs you can use for this, innosetup being one of them.

* How do I add accounts/characters?

In order to add accounts you can either do it manually in mysql, or use the script. If you followed my tutorial you should be able to simply use the scripts without any editing. If you changed mysql password, you have to do change that in create_account.c and create_character.c

Run the commands like this:

./create_account "email" "password"

./create_character "account ID" "NAME" "CLASS""GENDER"

eg:

./create_account newaccount@gmail.com 123456

./create_character 2 newmalemage MM

./create_character 2 newmalewarrior MW

./create_character 2 newfemalemage FM

./create_character 2 newfemalewarrior FW
