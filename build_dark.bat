gcc -o main main.c resource.o -lcrypt32 -lshell32 -lole32 -lgdi32 -luxtheme -ldwmapi -mwindows && strip main.exe
