# RAGEConverter GUI

## How to build?

This requires an extra step unlike the CLI version.

First, you need to build the resource file so the program can use the RAGE logo and the RAGE icon:

```ps1
windres -O coff -o resource.o resource.rc
```

Once you've done that, you build the actual program now:

```ps1
gcc -o main main.c resource.o -s -ladvapi32 -lgdi32 -lole32 -lcomdlg32 -luuid -mwindows
```
