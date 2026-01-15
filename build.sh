#!/bin/bash
# Script de construction SwiftVelox

echo "=== SWIFTVELOX BUILDER ==="
echo ""

# Vérifier les outils
echo "🔍 Vérification des outils..."
if command -v gcc >/dev/null 2>&1; then
    echo "✓ GCC trouvé"
else
    echo "⚠️  Installation de GCC..."
    if command -v apk >/dev/null 2>&1; then
        apk add gcc musl-dev
    elif command -v apt >/dev/null 2>&1; then
        apt update && apt install -y gcc
    elif command -v yum >/dev/null 2>&1; then
        yum install -y gcc
    else
        echo "❌ Impossible d'installer GCC"
        exit 1
    fi
fi

# Nettoyer
echo ""
echo "🧹 Nettoyage..."
make clean 2>/dev/null || true

# Compiler
echo ""
echo "🔨 Compilation..."
make

if [ $? -eq 0 ] && [ -f "./swiftvelox" ]; then
    echo ""
    echo "✅ SWIFTVELOX COMPILÉ AVEC SUCCÈS!"
    echo ""
    echo "📋 Fichiers disponibles:"
    ls -la examples/*.svx
    echo ""
    echo "🚀 Pour tester:"
    echo "  ./swiftvelox run examples/test.svx"
    echo "  ./swiftvelox run examples/calc.svx"
    echo "  ./swiftvelox run examples/hello.svx"
    echo ""
    echo "📝 Pour compiler votre propre programme:"
    echo "  ./swiftvelox build votre_fichier.svx"
    echo "  ./swiftvelox run votre_fichier.svx"
else
    echo "❌ Échec de la compilation"
    echo ""
    echo "Tentative manuelle:"
    gcc -O2 -o swiftvelox src/main.c
fi
