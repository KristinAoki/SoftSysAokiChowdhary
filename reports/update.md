# nucleus: a diy text editor

# Project Goal

For our project, we are going to build our own terminal-based text editor. Our MVP will be a simple editor that allows the user to write text and save this text to a file. The editor will allow the user to edit the text they wrote/rewrite it. We will also incorporate robust error handling as one of the skills that we hope to develop through this project. Other features we could implement could include keyboard shortcuts or a simple GUI, which might also incorporate visual features like syntax highlighting.

# Learning Goals

Shreya: My personal learning goals for this project are to gain practical experience with C so that I can become more comfortable with writing C code. I especially want to become more comfortable with pointers and memory allocation, as these are facets that are unique to C that I haven't dealt with as much in my past programming experience. A slightly less important but motivating goal for me is to build something with actual utility.

Kristin: My learning goals are to become more comfortable programming in C and to contribute to a project that is interesting and could possibly be used for future applications.

# Getting Started

## Resources

The main resource we're planning to utilize throughout this project is this tutorial: [Kilo Build Your Own Text Editor](https://viewsourcecode.org/snaptoken/kilo/index.html). We plan on going through the tutorial, aiming to understand each component of the tutorial thoroughly and improve the tutorial wherever we see possible.

Other resources that we will consult (related to the tutorial):
* [Documentation for the termios struct](http://man7.org/linux/man-pages/man3/termios.3.html)
* [Wikipedia article about escape sequences in C](https://en.wikipedia.org/wiki/Escape_sequences_in_C)
* [VT100 User Guide for more info about escape sequences](https://vt100.net/docs/vt100-ug/chapter3.html)

## What we've done so far
So far, we have created a text editor within the terminal that displays a welcome message to the user (see `editorDrawRows()`). The text editor also responds to various keypresses; for example, we have incorporated functionality for responding to the Page Up and Page Down keys and all the arrow keys so that the user can move the cursor wherever they want to in the terminal (see `editorMoveCursor()`). See `editorReadKey()` and `editorProcessKeypress()` for additional information. In addition, we have configured a few CTRL commands. For instance, the user can press CTRL+Q to exit the editor.

We configured these settings by first turning the terminal from canonical mode to raw mode by modifying the attributes to the terminal (by modifying the termios struct, which represents the characteristics of the terminal). We turned off most of the standard attributes of the terminal and reset these attributes ourselves. (See `disableRawMode()` and `enableRawMode()`). The characteristics of the terminal as well as various other important characteristics of the editor (like the position of the cursor and the current row) are encapsulated in the `Editor` struct we constructed.

We also have implemented functionality to read in the keys that the user presses and store these letters in a buffer string (which we encapsulated in the `Abuf` struct). See `editorProcessKeypress()` and `editorReadKey()` for further details of our implementation. However, we haven't yet implemented actually printing these letters out to the terminal so that the user can see what they're typing.

In the next few steps of the tutorial, we are going to implement the following features:
* Opening a text file and displaying the contents in the editor (in progress--see `editorOpen()`)
* Writing multiple lines of text to the file (and displaying these lines as they're being written)
* Vertical and horizontal scrolling through the file
* A status bar to show useful information about the file, like the filename, the number of lines in the file, and which line you are currently on
* Displaying the letters that the user is pressing on the screen, and allowing the user to press the backspace key to delete characters
* Allowing the user to use the Enter key to go to new lines
* Saving a file to the disk (with a specific name too--"Save as" functionality)
* Seeking confirmation from the user before they quit the editor (especially if they haven't saved yet)

One major improvement that we are thinking of implementing is separating some of the functionality into separate files. We believe that this exercise will also help us learn how to work with multiple C files and practice proper modular design. We plan to restructure the code into various C files after we have completed the tutorial, as this exercise will also help us ensure that we actually completely understand the tutorial.

We aim to complete these tasks over the course of two sprints. We have divided each step of the tutorial and all the substeps in each step between both of us; typically, whenever one person completes their part, the other person reviews the tutorial and code and writes documentation to understand exactly how that part functions. This process has worked well for us so far and we plan to continue using it.
