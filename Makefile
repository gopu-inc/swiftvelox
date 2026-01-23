# SwiftFlow Makefile
# Structure: src/*.c, include/*.h

# ======================================================
# [SECTION] CONFIGURATION
# ======================================================
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g -I./include
LDFLAGS = -lm
TARGET = swiftflow

# ======================================================
# [SECTION] SOURCE FILES
# ======================================================
# Fichiers source essentiels (toujours compilés)
CORE_SRCS = src/main.c \
            src/lexer.c \
            src/parser.c \
            src/ast.c \
            src/interpreter.c \
            src/jsonlib.c \
            src/swf.c \
            src/llvm_backend.c \
            src/keywords.c

# Fichiers source optionnels (selon les dépendances)
OPTIONAL_SRCS = 

# Fichiers objets correspondants
CORE_OBJS = $(CORE_SRCS:.c=.o)
OPTIONAL_OBJS = $(OPTIONAL_SRCS:.c=.o)

# ======================================================
# [SECTION] DÉTECTION DES DÉPENDANCES
# ======================================================
# Détection de PCRE (expressions régulières)
HAS_PCRE = $(shell pkg-config --exists libpcre 2>/dev/null && echo 1 || echo 0)
ifeq ($(HAS_PCRE),1)
CFLAGS += -DHAVE_PCRE
LDFLAGS += -lpcre
OPTIONAL_SRCS += src/regexlib.c
endif

# Détection de LibTomMath (mathématiques avancées)
HAS_TOMMATH = $(shell pkg-config --exists libtommath 2>/dev/null && echo 1 || echo 0)
ifeq ($(HAS_TOMMATH),1)
CFLAGS += -DHAVE_TOMMATH
LDFLAGS += -ltommath
OPTIONAL_SRCS += src/mathlib.c
endif

# Détection de Readline (REPL amélioré)
HAS_READLINE = $(shell pkg-config --exists readline 2>/dev/null && echo 1 || echo 0)
ifeq ($(HAS_READLINE),1)
CFLAGS += -DHAVE_READLINE
LDFLAGS += -lreadline
OPTIONAL_SRCS += src/repl.c
else
# Fallback: utiliser le REPL simple
CFLAGS += -DHAVE_SIMPLE_REPL
endif

# Tous les fichiers objets
OBJS = $(CORE_OBJS) $(OPTIONAL_OBJS)

# ======================================================
# [SECTION] RÈGLES DE COMPILATION
# ======================================================
all: $(TARGET)

$(TARGET): $(OBJS)
	@echo "📦 Édition des liens..."
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "✅ SwiftFlow compilé avec succès!"
	@echo "📊 Dépendances détectées:"
	@echo "   PCRE (regex): $(HAS_PCRE)"
	@echo "   LibTomMath: $(HAS_TOMMATH)"
	@echo "   Readline: $(HAS_READLINE)"

# ======================================================
# [SECTION] RÈGLES SPÉCIFIQUES
# ======================================================
# Règle générique pour les fichiers .c -> .o
%.o: %.c
	@echo "🔨 Compilation de $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Règles spécifiques avec dépendances
src/main.o: src/main.c include/common.h include/lexer.h include/parser.h include/ast.h include/interpreter.h
	@echo "🔨 Compilation de src/main.c..."
	$(CC) $(CFLAGS) -c $< -o $@

src/lexer.o: src/lexer.c include/lexer.h include/common.h
	@echo "🔨 Compilation de src/lexer.c..."
	$(CC) $(CFLAGS) -c $< -o $@

src/parser.o: src/parser.c include/parser.h include/common.h include/ast.h
	@echo "🔨 Compilation de src/parser.c..."
	$(CC) $(CFLAGS) -c $< -o $@

src/ast.o: src/ast.c include/ast.h include/common.h
	@echo "🔨 Compilation de src/ast.c..."
	$(CC) $(CFLAGS) -c $< -o $@

src/interpreter.o: src/interpreter.c include/interpreter.h include/common.h
	@echo "🔨 Compilation de src/interpreter.c..."
	$(CC) $(CFLAGS) -c $< -o $@

src/jsonlib.o: src/jsonlib.c include/jsonlib.h include/common.h include/interpreter.h
	@echo "🔨 Compilation de src/jsonlib.c..."
	$(CC) $(CFLAGS) -c $< -o $@

src/swf.o: src/swf.c include/common.h
	@echo "🔨 Compilation de src/swf.c..."
	$(CC) $(CFLAGS) -c $< -o $@

src/llvm_backend.o: src/llvm_backend.c include/backend.h include/common.h
	@echo "🔨 Compilation de src/llvm_backend.c..."
	$(CC) $(CFLAGS) -c $< -o $@

src/keywords.o: src/keywords.c include/common.h
	@echo "🔨 Compilation de src/keywords.c..."
	$(CC) $(CFLAGS) -c $< -o $@

# Règles pour fichiers optionnels
src/regexlib.o: src/regexlib.c include/common.h
	@echo "🔨 Compilation de src/regexlib.c..."
	$(CC) $(CFLAGS) -c $< -o $@

src/mathlib.o: src/mathlib.c include/common.h
	@echo "🔨 Compilation de src/mathlib.c..."
	$(CC) $(CFLAGS) -c $< -o $@

src/repl.o: src/repl.c include/common.h
	@echo "🔨 Compilation de src/repl.c..."
	$(CC) $(CFLAGS) -c $< -o $@

# ======================================================
# [SECTION] RÈGLES UTILITAIRES
# ======================================================
clean:
	@echo "🧹 Nettoyage des fichiers objets et exécutable..."
	rm -f $(OBJS) $(TARGET)
	@echo "✅ Nettoyage terminé!"

clean-all: clean
	@echo "🧹 Nettoyage complet..."
	rm -f *.o *~ core *.swp
	find . -name "*.o" -delete
	find . -name "*~" -delete
	find . -name "*.swp" -delete
	@echo "✅ Nettoyage complet terminé!"

install: $(TARGET)
	@echo "📦 Installation de SwiftFlow..."
	cp $(TARGET) /usr/local/bin/swiftflow
	mkdir -p /usr/local/include/swiftflow
	cp include/*.h /usr/local/include/swiftflow/
	@echo "✅ SwiftFlow installé dans /usr/local/bin/"
	@echo "   Headers dans /usr/local/include/swiftflow/"

uninstall:
	@echo "🗑️  Désinstallation de SwiftFlow..."
	rm -f /usr/local/bin/swiftflow
	rm -rf /usr/local/include/swiftflow
	@echo "✅ SwiftFlow désinstallé!"

# Installation des dépendances (Ubuntu/Debian)
install-deps:
	@echo "📦 Installation des dépendances..."
	apt-get update
	apt-get install -y gcc make
	apt-get install -y libpcre3-dev libtommath-dev libreadline-dev
	@echo "✅ Dépendances installées!"

# Installation des dépendances (macOS)
install-deps-macos:
	@echo "📦 Installation des dépendances (macOS)..."
	brew update
	brew install gcc make pcre libtommath readline
	@echo "✅ Dépendances installées!"

# Test basique
test: $(TARGET)
	@echo "🧪 Test de compilation..."
	@echo "print('Hello SwiftFlow!')" > test_hello.swf
	./$(TARGET) test_hello.swf
	rm -f test_hello.swf

# Test avancé
test-advanced: $(TARGET)
	@echo "🧪 Test avancé..."
	@echo "# Test SwiftFlow\nvar x = 10 + 20\nprint('Result:', x)\nif [x > 20] { print('x > 20') }" > test_advanced.swf
	./$(TARGET) test_advanced.swf
	rm -f test_advanced.swf

# Mode debug
debug: CFLAGS += -DDEBUG -O0 -ggdb3
debug: clean $(TARGET)
	@echo "🐛 Version debug compilée!"

# Mode release
release: CFLAGS += -O3 -DNDEBUG
release: LDFLAGS += -s
release: clean $(TARGET)
	@echo "🚀 Version release compilée!"

# Afficher les informations de configuration
info:
	@echo "📊 Configuration SwiftFlow:"
	@echo "   Compilateur: $(CC)"
	@echo "   Flags: $(CFLAGS)"
	@echo "   LD Flags: $(LDFLAGS)"
	@echo "   Cibles: $(CORE_SRCS)"
	@echo "   Dépendances:"
	@echo "     PCRE: $(HAS_PCRE)"
	@echo "     LibTomMath: $(HAS_TOMMATH)"
	@echo "     Readline: $(HAS_READLINE)"

# Créer un package source
dist: clean-all
	@echo "📦 Création du package source..."
	mkdir -p dist/swiftflow
	cp -r src include Makefile README.md LICENSE test.swf dist/swiftflow/
	tar -czf swiftflow-$(shell date +%Y%m%d).tar.gz -C dist swiftflow
	rm -rf dist
	@echo "✅ Package créé: swiftflow-$(shell date +%Y%m%d).tar.gz"

# Vérifier la syntaxe des fichiers
check-syntax:
	@echo "🔍 Vérification de la syntaxe..."
	$(CC) $(CFLAGS) -fsyntax-only $(CORE_SRCS) $(OPTIONAL_SRCS)
	@echo "✅ Syntaxe OK!"

# Formatage du code (si clang-format est disponible)
format:
	@echo "🎨 Formatage du code..."
	@if command -v clang-format >/dev/null 2>&1; then \
		find src include -name "*.c" -o -name "*.h" | xargs clang-format -i; \
		echo "✅ Code formaté!"; \
	else \
		echo "⚠️  clang-format non installé. Installer avec: apt-get install clang-format"; \
	fi

# Nettoyer les balises [file name]: etc.
clean-tags:
	@echo "🧹 Nettoyage des balises des fichiers..."
	@find src include -type f \( -name "*.c" -o -name "*.h" \) -exec sed -i '/^\[file name\]:/d;/^\[file content begin\]/d;/^\[file content end\]/d' {} \;
	@echo "✅ Balises nettoyées!"

# Aide
help:
	@echo "🎯 SwiftFlow Makefile - Commandes disponibles:"
	@echo ""
	@echo "🔨 Construction:"
	@echo "  make           - Compiler SwiftFlow"
	@echo "  make debug     - Compiler en mode debug"
	@echo "  make release   - Compiler en mode release"
	@echo ""
	@echo "🧹 Nettoyage:"
	@echo "  make clean     - Nettoyer les fichiers objets"
	@echo "  make clean-all - Nettoyer complètement"
	@echo "  make clean-tags- Nettoyer les balises [file name]:"
	@echo ""
	@echo "📦 Installation:"
	@echo "  make install   - Installer dans /usr/local/bin"
	@echo "  make uninstall - Désinstaller"
	@echo "  make install-deps - Installer les dépendances (Ubuntu)"
	@echo ""
	@echo "🧪 Tests:"
	@echo "  make test      - Test simple"
	@echo "  make test-advanced - Test avancé"
	@echo "  make check-syntax - Vérifier la syntaxe"
	@echo ""
	@echo "📊 Utilitaires:"
	@echo "  make info      - Afficher les informations"
	@echo "  make format    - Formater le code (si clang-format)"
	@echo "  make dist      - Créer un package source"
	@echo "  make help      - Afficher cette aide"
	@echo ""

# ======================================================
# [SECTION] DÉPENDANCES SPÉCIFIQUES
# ======================================================
# Dépendances des headers
include/common.h:
include/lexer.h: include/common.h
include/parser.h: include/common.h include/ast.h
include/ast.h: include/common.h
include/interpreter.h: include/common.h
include/jsonlib.h: include/common.h
include/backend.h: include/common.h include/ast.h

# Phony targets
.PHONY: all clean clean-all install uninstall install-deps install-deps-macos \
        test test-advanced debug release info dist check-syntax format clean-tags help
