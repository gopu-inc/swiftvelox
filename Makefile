# Compilateur et options
CC = cc
CFLAGS = -std=c99 -g -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wno-format-truncation

# Bibliothèques à lier (-lcurl est essentiel pour http.c)
LIBS = -lm -lsqlite3 -lcurl

# Liste des fichiers objets
OBJS = swf.o lexer.o parser.o io.o net.o sys.o http.o json.o stdlib.o

# Cible par défaut
all: swift

# Édition de liens pour créer l'exécutable
swift: $(OBJS)
	$(CC) $(CFLAGS) -o swift $(OBJS) $(LIBS)

# Règles de compilation pour chaque module
swf.o: swf.c common.h io.h net.h sys.h http.h json.h
	$(CC) $(CFLAGS) -c swf.c -o swf.o

stdlib.o: stdlib.c common.h stdlib.h
	$(CC) $(CFLAGS) -c stdlib.c -o stdlib.o

lexer.o: lexer.c common.h
	$(CC) $(CFLAGS) -c lexer.c -o lexer.o

parser.o: parser.c common.h
	$(CC) $(CFLAGS) -c parser.c -o parser.o

io.o: io.c common.h io.h
	$(CC) $(CFLAGS) -c io.c -o io.o

net.o: net.c common.h net.h
	$(CC) $(CFLAGS) -c net.c -o net.o

sys.o: sys.c common.h sys.h
	$(CC) $(CFLAGS) -c sys.c -o sys.o

http.o: http.c common.h http.h
	$(CC) $(CFLAGS) -c http.c -o http.o

json.o: json.c common.h json.h
	$(CC) $(CFLAGS) -c json.c -o json.o

# Nettoyage
clean:
	rm -f *.o swift
