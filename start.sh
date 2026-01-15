#!/bin/sh
echo "=== SwiftVelox ==="
echo "Compilation..."
make

if [ -f "./svc" ]; then
    echo "✅ Compilateur créé: ./svc"
    echo ""
    echo "Test:"
    ./svc run examples/test.svx
else
    echo "❌ Erreur"
    exit 1
fi
