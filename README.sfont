-----------------------------------< SFont >------------------------------------

License: GPL
Author: Karl Bartel <karlb@gmx.net>
WWW: http://members.linuxstart.com/~karlb/

Usage:
    1. Load a font and place it in an SDL_Surface *. If your are using the 
    SDL_image library you can do this with Font=IMG_Load("filename");
    2. Initialize the font by using InitFont(surface *Font);
    3. Now you can use the font in you SDL program.
       Please take a look at SFont.h to get info about the funtions.

FileFormat:
    The font file can be any type image file. The characters start with
    ASCII symbol #33. They are seperated by pink(255,0,255) lines at the top
    of the image. The space between these lines is the width of the caracter.
    Just take a look at the font, and you'll be able to understand what I tried
    to explain here.
    The easiest way to create a new font is to use the GIMP's Logo function.
    Use the following string as text (ASCII 33-127 with escape sequences and
    spaces between the letters):
    ! \" # $ % & ' ( ) * + , - . / 0 1 2 3 4 5 6 7 8 9 : ; < = > ? @ A B C D E F G H I J K L M N O P Q R S T U V W X Y Z [ \\ ] ^ _ ` a b c d e f g h i j k l m n o p q r s t u v w x y z { | } ~

Example for the font file format:

____       _____      _____     _____     <- This is the pink line at the top

       #        #####       ####
      # #       #    #     #
      # #       #    #     #
     #   #      #####      #
     #####      #    #     #
    #     #     #    #     #
    #     #     #####       ####

		|----|
	Width of the character
             
	     |----------|
Part of the font surface that is beeing blitted


ChangeLog:
2.0 beta
    *** Added SoFont, a C++ version of SFont ***
    Added lock and unlock for hardware surfaces
    Added blinking cursor to SFont_Input
    Added check for ASCII<33 in SFont_Input
    TextWidth returned a too highh value -> fixed

1.4
    Added support for multiple fonts in one program
    Add a missing "*" in the SFont.h file
    Proper '\0' at the end of stings produced by SFont_Input
    Fixed problems in SFont_Input, which were creating '\b' in the string
    Fixed two other bugs in SFont_Input
    Added the SFontViewer adn new test programs

1.3
    Fixed three wrong interpretations of the file format
	o wrong division (float had to be used)
	o detection of right ends of pink lines (were one pixel too far right)
	o blit destination (all blits were too far right)
    support for non-alpha mapped fonts
    some fonts updated
    
1.2
    added XCenteredString

1.1
    renamed font.c to SFont.c and font.h to SFont.h
    better Makefile
1.0
    Added the Input function
    Added a check wether the Font has been loaded
0.9
    better Makefile for the example (using sdl-config)
    better Neon Font
0.8 
    First Release

If you have any Questions please write a mail!

Karl