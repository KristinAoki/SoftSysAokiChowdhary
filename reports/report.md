# nucleus: a diy text editor
## Shreya Chowdhary and Kristin Aoki

# Project Goal

For our project, we are going to build our own terminal-based text editor. Our MVP will be a simple editor that can read, write, and append to files. Ideally, this would include allowing the user to open and edit as many files as they like before closing the text editor. We also aim to incorporate robust error handling into our system. Other features we could implement could include keyboard shortcuts or a simple GUI to make it more user-friendly. With a simple GUI, we could also work on including other visual features like syntax highlighting, which is a useful feature of many text editors.

# Learning Goals

Shreya: My personal learning goals for this project are to gain practical experience with C so that I can become more comfortable with writing C code. I especially want to become more comfortable with pointers and memory allocation, as these are facets that are unique to C that I haven't dealt with as much in my past programming experience. A slightly less important but motivating goal for me is to build something with actual utility.

Kristin: My learning goals are to become more comfortable programming in C and to contribute to a project that is interesting and could possibly be used for future applications.

# Resources

The main resource we're planning to utilize throughout this project is this tutorial: [Kilo Build Your Own Text Editor](https://viewsourcecode.org/snaptoken/kilo/index.html). We plan on going through the tutorial, aiming to understand each component of the tutorial thoroughly and improve the tutorial wherever we see possible.

Other resources that we will consult (related to the tutorial):
* [Documentation for the termios struct](http://man7.org/linux/man-pages/man3/termios.3.html)
* [Wikipedia article about escape sequences in C](https://en.wikipedia.org/wiki/Escape_sequences_in_C)
* [VT100 User Guide for more info about escape sequences](https://vt100.net/docs/vt100-ug/chapter3.html)

# What We Achieved
Over the course of this project, we created a text editor within the terminal that allows a user to edit existing files or create new files. Like any text editor, the editor allows a user to insert characters or new lines anywhere, edit characters, and delete any characters or lines. This process involved implementing a robust key-processing function, `editorProcessKeypress()`, that calls upon various other functions that adjust the rows of the editor in a robust way. We also built in additional user interactivity by configuring some CTRL commands (like CTRL+Q to allow the user to exit an editor), allowing the user to use the arrow keys to jump aroud the file, and displaying a status message that indicates how many lines there are in the file and whether the file has been changed. We also integrated robust error handling in all parts of the editor.

For our implementation, we designed a `Editor` struct that represents the editor. This struct contains fields like a representation of the original terminal (because we reconfigure the terminal in order to give the user more control), trackers for the cursor position and where to start rendering characters, information about the number of rows in the editor, information about the file (like the filename or whether it has been changed). It also contains an array of rows in the editor, which are represented using the `Erow` struct. Every `Erow` is defined by the size of the string inside the row and the size of the string to be rendered (in case there were tabs). A third struct we use is the `Abuf` struct, which represents a buffer that contains the new contents to be added.

First, we enable row mode and initialize the editor (using the `enableRawMode` and `initEditor` functions). In these functions, we take control of the terminal and set up the editor. We open a file if the user passed a file (using the `editorOpen` function). Then, we continuously refresh the screen (using the `editorRefreshScreen` function) and process the user's keypress (using the `editorProcessKeypress` function).

When we refresh the screen, we scroll the editor to the correct position. Then we clear the screen using an escape sequence, draw the rows, status bar, and message bar, and then reposition the cursor. We write our buffer's contents to the file and then clear our buffer.

# Design Decisions We made

Because we were following a tutorial, we followed the design decisions that the creators of the tutorial made, so that we could focus on learning C and solidifying our understanding of the various concepts we had covered so far. However, we did reflect on the design decisions that the creators of the tutorial made and thought about some of the things we could change if we were to iterate upon the editor (some of which we had planned to do had we had time to do so).

One design decision that the tutorial made was representing the rows in the editor as an array of `Erow` objects. An alternative structure could've been using a doubly linked list to represent the rows. This might've been beneficial especially in deleting or inserting rows if we were given a specific row. However, it would've made it more difficult to access a specific row, which was a necessary functionality as we wanted to allow the user to be able to freely navigate the contents of the editor. In order to do that with a linked list, we would need to have a pointer storing the current row, in which case we would be growing closer and closer to essentially implementing an array. Therefore, an array ended up being the better design.

Another possible design decision could've been creating prototype-based objects for all the structs, especially the Editor and Erow structs. This might've been a good idea because each of these objects had a distinct set of functions specific to these objects. A future extension could be to implement this structure and then also separate the Editor and Erow structs to different header files to essentially create a library for acting with these structures. This could be helpful if someone wanted to build upon this editor and create, for example, a collaborative editor, or add other features to the editor.


# Reflection

Shreya: I feel like this project met and even exceeded my goals. I had to become very familiar with memory allocation and pointers given the number of different structures that were used in the editor. I also learned about a lot of functions of C that I hadn't expected to learn about, like escape sequences. I feel very comfortable with working with structs that interact in complex ways now, which is a useful skill to have developed.

Kristin: I believe that I met my learning goals for this project. Throughout the project my C knowledge was challenged and improved. It was really nice when the functions that we created were the came things that we were learning about in class at the same time. This allowed me to have a better understanding of the concepts in class. This project also allowed me to learn about topics that we did not discuss in class, such as escape sequences.  In addition, it was really cool to see the project come together and make a text editor.
