# RAGEConverter

## What is it?
RAGEConverter is a GUI program that lets you convert RAGE screenshot files to actual images.

It supports **both GTA V and RDR2 screenshots**.

## How does it work?
It works by scanning the input directory for files that have the JPEG magic header and converts them to actual JPEG images.

## Where are my screenshots located?
Your screenshots are located in different places:

For GTA V:
`C:\Users\<USERNAME>\Documents\Rockstar Games\GTA V\Profiles\<PROFILE>`

For Enhanced:
`C:\Users\<USERNAME>\Documents\Rockstar Games\GTAV Enhanced\Profiles\<PROFILE>`

For RDR2:
`C:\Users\<USERNAME>\Documents\Rockstar Games\Red Dead Redemption 2\Profiles\<PROFILE>`

## How to use?
All you have to do is select your input folder, AKA where your screenshots are located, select your output folder (optional), and click "Convert".

If you don't select an output folder, the program will automatically make a folder named "Converted" inside the folder you ran the program in.

You can watch the program convert the photos real time.

### GUI vs Command Line
There is also a command line version, and it defaults to "./Photos" and "./Converted", for all the people who may want to make scripts using this tool.

## Compatibility
This program is written purely in C along with the Win32 API, and it's compatible with older Windows versions all the way up to **Windows XP**.

## Compiling
You can see the instructions for compiling both the CLI and the GUI versions of the program in their corresponding directories.

If you're too lazy to compile for yourself, you can just get the precompiled binary in the [Releases](https://github.com/NotStrahinja/RAGEConverter/releases/latest) tab.

## Screenshots

<img width="494" height="396" alt="image" src="https://github.com/user-attachments/assets/7e945919-6322-4712-a834-94b3e24c000a" />


> [!NOTE]
> The program will keep your original screenshot files in tact and it should also work with future titles (like GTA VI).
